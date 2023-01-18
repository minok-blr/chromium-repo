// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_queue.h"

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/compression/decompression.h"
#include "components/reporting/encryption/test_encryption_module.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_interface.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Between;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::WithoutArgs;

namespace reporting {
namespace {

// Test uploader counter - for generation of unique ids.
std::atomic<int64_t> next_uploader_id{0};

constexpr size_t kCompressionThreshold = 2;
const CompressionInformation::CompressionAlgorithm kCompressionType =
    CompressionInformation::COMPRESSION_SNAPPY;

// Metadata file name prefix.
const base::FilePath::CharType METADATA_NAME[] = FILE_PATH_LITERAL("META");

// Forbidden file/folder names
const base::FilePath::StringType kInvalidFilePrefix = FILE_PATH_LITERAL("..");
#if BUILDFLAG(IS_WIN)
const base::FilePath::StringPieceType kInvalidDirectoryPath =
    FILE_PATH_LITERAL("o:\\some\\inaccessible\\dir");
#else
const base::FilePath::StringPieceType kInvalidDirectoryPath =
    FILE_PATH_LITERAL("////////////");
#endif

// Ensure files as specified by the parameters are deleted. Take the same
// parameters as base::FileEnumerator().
template <typename... FileEnumeratorParams>
void EnsureDeletingFiles(FileEnumeratorParams... file_enum_params) {
  base::FileEnumerator dir_enum(file_enum_params...);
  ASSERT_TRUE(DeleteFilesWarnIfFailed(dir_enum));
  // Ensure that the files have been deleted
  ASSERT_TRUE(base::FileEnumerator(
                  std::forward<FileEnumeratorParams>(file_enum_params)...)
                  .Next()
                  .empty());
}

class StorageQueueTest
    : public ::testing::TestWithParam<
          testing::tuple<size_t /*file_size*/, std::string /*dm_token*/>> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
    dm_token_ = testing::get<1>(GetParam());
    options_.set_directory(location_.GetPath());
    // Disallow uploads unless other expectation is set (any later EXPECT_CALL
    // will take precedence over this one).
    EXPECT_CALL(set_mock_uploader_expectations_, Call(_))
        .WillRepeatedly(Invoke([](UploaderInterface::UploadReason reason) {
          return Status(
              error::UNAVAILABLE,
              base::StrCat({"Test uploader not provided by the test, reason=",
                            UploaderInterface::ReasonToString(reason)}));
        }));
  }

  void TearDown() override {
    ResetTestStorageQueue();
    // Make sure all memory is deallocated.
    ASSERT_THAT(options_.memory_resource()->GetUsed(), Eq(0u));
    // Make sure all disk is not reserved (files remain, but Storage is not
    // responsible for them anymore).
    ASSERT_THAT(options_.disk_space_resource()->GetUsed(), Eq(0u));
    // Log next uploader id for possible verification.
    LOG(ERROR) << "Next uploader id=" << next_uploader_id.load();
  }

  // Mock class used for setting upload expectations on it.
  class MockUpload {
   public:
    MockUpload() = default;
    virtual ~MockUpload() = default;
    MOCK_METHOD(void,
                EncounterSeqId,
                (int64_t /*uploader_id*/, int64_t),
                (const));
    MOCK_METHOD(bool,
                UploadRecord,
                (int64_t /*uploader_id*/, int64_t, base::StringPiece),
                (const));
    MOCK_METHOD(bool,
                UploadRecordFailure,
                (int64_t /*uploader_id*/, int64_t, Status),
                (const));
    MOCK_METHOD(bool,
                UploadGap,
                (int64_t /*uploader_id*/, int64_t, uint64_t),
                (const));
    MOCK_METHOD(void,
                UploadComplete,
                (int64_t /*uploader_id*/, Status),
                (const));
  };

  // Helper class to be wrapped in SequenceBound<..>, in order to make sure
  // all its methods are run on a main sequential task wrapper. As a result,
  // collected information and EXPECT_CALLs to MockUpload are safe - executed on
  // the main test thread.
  class SequenceBoundUpload {
   public:
    explicit SequenceBoundUpload(const MockUpload* mock_upload)
        : mock_upload_(mock_upload) {
      DETACH_FROM_SEQUENCE(scoped_checker_);
      upload_progress_.assign("\nStart\n");
    }
    SequenceBoundUpload(const SequenceBoundUpload& other) = delete;
    SequenceBoundUpload& operator=(const SequenceBoundUpload& other) = delete;
    ~SequenceBoundUpload() { DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_); }

    void DoEncounterSeqId(int64_t uploader_id, int64_t sequencing_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("SeqId: ")
          .append(base::NumberToString(sequencing_id))
          .append("\n");
      mock_upload_->EncounterSeqId(uploader_id, sequencing_id);
    }

    void DoUploadRecord(int64_t uploader_id,
                        int64_t sequencing_id,
                        base::StringPiece data,
                        base::OnceCallback<void(bool)> processed_cb) {
      DoEncounterSeqId(uploader_id, sequencing_id);
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("Record: ")
          .append(base::NumberToString(sequencing_id))
          .append(" '")
          .append(data.data(), data.size())
          .append("'\n");
      std::move(processed_cb)
          .Run(mock_upload_->UploadRecord(uploader_id, sequencing_id, data));
    }

    void DoUploadRecordFailure(int64_t uploader_id,
                               int64_t sequencing_id,
                               Status status,
                               base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("Failure: ")
          .append(base::NumberToString(sequencing_id))
          .append(" '")
          .append(status.ToString())
          .append("'\n");
      std::move(processed_cb)
          .Run(mock_upload_->UploadRecordFailure(uploader_id, sequencing_id,
                                                 status));
    }

    void DoUploadGap(int64_t uploader_id,
                     int64_t sequencing_id,
                     uint64_t count,
                     base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      for (uint64_t c = 0; c < count; ++c) {
        DoEncounterSeqId(uploader_id, sequencing_id + static_cast<int64_t>(c));
      }
      upload_progress_.append("Gap: ")
          .append(base::NumberToString(sequencing_id))
          .append("(")
          .append(base::NumberToString(count))
          .append(")\n");
      std::move(processed_cb)
          .Run(mock_upload_->UploadGap(uploader_id, sequencing_id, count));
    }

    void DoUploadComplete(int64_t uploader_id, Status status) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("Complete: ")
          .append(status.ToString())
          .append("\n");
      LOG(ERROR) << "TestUploader: " << upload_progress_ << "End\n";
      mock_upload_->UploadComplete(uploader_id, status);
    }

   private:
    const raw_ptr<const MockUpload> mock_upload_;

    SEQUENCE_CHECKER(scoped_checker_);

    // Snapshot of data received in this upload (for debug purposes).
    std::string upload_progress_;
  };

  // Uploader interface implementation to be assigned to tests.
  // Note that Storage guarantees that all APIs are executed on the same
  // sequenced task runner (not the main test thread!).
  class TestUploader : public UploaderInterface {
   public:
    // Mapping of <generation id, sequencing id> to matching record digest.
    // Whenever a record is uploaded and includes last record digest, this map
    // should have that digest already recorded. Only the first record in a
    // generation is uploaded without last record digest. "Optional" is set to
    // no-value if there was a gap record instead of a real one.
    using LastRecordDigestMap = base::flat_map<
        std::pair<int64_t /*generation id */, int64_t /*sequencing id*/>,
        absl::optional<std::string /*digest*/>>;

    // Helper class for setting up mock uploader expectations of a successful
    // completion.
    class SetUp {
     public:
      SetUp(test::TestCallbackWaiter* waiter, StorageQueueTest* self)
          : uploader_(std::make_unique<TestUploader>(self)),
            uploader_id_(uploader_->uploader_id_),
            waiter_(waiter) {}
      SetUp(const SetUp& other) = delete;
      SetUp& operator=(const SetUp& other) = delete;
      ~SetUp() { CHECK(!uploader_) << "Missed 'Complete' call"; }

      std::unique_ptr<TestUploader> Complete(
          Status status = Status::StatusOK()) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadComplete(Eq(uploader_id_), Eq(status)))
            .InSequence(uploader_->test_upload_sequence_,
                        uploader_->test_encounter_sequence_)
            .WillOnce(DoAll(
                WithoutArgs(
                    Invoke(waiter_.get(), &test::TestCallbackWaiter::Signal)),
                WithArg<1>(Invoke([](Status status) {
                  LOG(ERROR) << "Completion signaled with status=" << status;
                })),
                WithoutArgs(
                    Invoke([]() { LOG(ERROR) << "Completion signaled"; }))));
        return std::move(uploader_);
      }

      SetUp& Required(int64_t sequencing_id, base::StringPiece value) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_id_), Eq(sequencing_id),
                                 StrEq(std::string(value))))
            .InSequence(uploader_->test_upload_sequence_)
            .WillOnce(Return(true));
        return *this;
      }

      SetUp& Possible(int64_t sequencing_id, base::StringPiece value) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_id_), Eq(sequencing_id),
                                 StrEq(std::string(value))))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_upload_sequence_)
            .WillRepeatedly(Return(true));
        return *this;
      }

      SetUp& RequiredGap(int64_t sequencing_id, uint64_t count) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadGap(Eq(uploader_id_), Eq(sequencing_id), Eq(count)))
            .InSequence(uploader_->test_upload_sequence_)
            .WillOnce(Return(true));
        return *this;
      }

      SetUp& PossibleGap(int64_t sequencing_id, uint64_t count) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadGap(Eq(uploader_id_), Eq(sequencing_id), Eq(count)))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_upload_sequence_)
            .WillRepeatedly(Return(true));
        return *this;
      }

      SetUp& Failure(int64_t sequencing_id, Status error) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(
            *uploader_->mock_upload_,
            UploadRecordFailure(Eq(uploader_id_), Eq(sequencing_id), Eq(error)))
            .InSequence(uploader_->test_upload_sequence_)
            .WillOnce(Return(true));
        return *this;
      }

      // The following two expectations refer to the fact that specific
      // sequencing ids have been encountered, regardless of whether they
      // belonged to records or gaps. The expectations are set on a separate
      // test sequence.
      SetUp& RequiredSeqId(int64_t sequencing_id) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    EncounterSeqId(Eq(uploader_id_), Eq(sequencing_id)))
            .Times(1)
            .InSequence(uploader_->test_encounter_sequence_);
        return *this;
      }

      SetUp& PossibleSeqId(int64_t sequencing_id) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    EncounterSeqId(Eq(uploader_id_), Eq(sequencing_id)))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_encounter_sequence_);
        return *this;
      }

     private:
      std::unique_ptr<TestUploader> uploader_;
      const int64_t uploader_id_;
      const raw_ptr<test::TestCallbackWaiter> waiter_;
    };

    explicit TestUploader(StorageQueueTest* self)
        : uploader_id_(next_uploader_id.fetch_add(1)),
          last_record_digest_map_(&self->last_record_digest_map_),
          mock_upload_(&self->mock_upload_),
          sequence_bound_upload_(self->main_task_runner_, &self->mock_upload_) {
      DETACH_FROM_SEQUENCE(test_uploader_checker_);
    }

    ~TestUploader() override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
    }

    void ProcessRecord(EncryptedRecord encrypted_record,
                       ScopedReservation scoped_reservation,
                       base::OnceCallback<void(bool)> processed_cb) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      auto sequence_information = encrypted_record.sequence_information();
      // Decompress encrypted_wrapped_record if is was compressed.
      WrappedRecord wrapped_record;
      if (encrypted_record.has_compression_information()) {
        std::string decompressed_record = Decompression::DecompressRecord(
            encrypted_record.encrypted_wrapped_record(),
            encrypted_record.compression_information());
        encrypted_record.set_encrypted_wrapped_record(decompressed_record);
      }
      ASSERT_TRUE(wrapped_record.ParseFromString(
          encrypted_record.encrypted_wrapped_record()));

      // Verify compression information is enabled or disabled.
      if (CompressionModule::is_enabled()) {
        EXPECT_TRUE(encrypted_record.has_compression_information());
      } else {
        EXPECT_FALSE(encrypted_record.has_compression_information());
      }

      VerifyRecord(std::move(sequence_information), std::move(wrapped_record),
                   std::move(processed_cb));
    }

    void ProcessGap(SequenceInformation sequence_information,
                    uint64_t count,
                    base::OnceCallback<void(bool)> processed_cb) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      // Verify generation match.
      if (generation_id_.has_value() &&
          generation_id_.value() != sequence_information.generation_id()) {
        sequence_bound_upload_
            .AsyncCall(&SequenceBoundUpload::DoUploadRecordFailure)
            .WithArgs(uploader_id_, sequence_information.sequencing_id(),
                      Status(error::DATA_LOSS,
                             base::StrCat(
                                 {"Generation id mismatch, expected=",
                                  base::NumberToString(generation_id_.value()),
                                  " actual=",
                                  base::NumberToString(
                                      sequence_information.generation_id())})),
                      std::move(processed_cb));
        return;
      }
      if (!generation_id_.has_value()) {
        generation_id_ = sequence_information.generation_id();
      }

      last_record_digest_map_->emplace(
          std::make_pair(sequence_information.sequencing_id(),
                         sequence_information.generation_id()),
          absl::nullopt);

      sequence_bound_upload_.AsyncCall(&SequenceBoundUpload::DoUploadGap)
          .WithArgs(uploader_id_, sequence_information.sequencing_id(), count,
                    std::move(processed_cb));
    }

    void Completed(Status status) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      sequence_bound_upload_.AsyncCall(&SequenceBoundUpload::DoUploadComplete)
          .WithArgs(uploader_id_, status);
    }

   private:
    void VerifyRecord(SequenceInformation sequence_information,
                      WrappedRecord wrapped_record,
                      base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      // Verify generation match.
      if (generation_id_.has_value() &&
          generation_id_.value() != sequence_information.generation_id()) {
        sequence_bound_upload_
            .AsyncCall(&SequenceBoundUpload::DoUploadRecordFailure)
            .WithArgs(uploader_id_, sequence_information.sequencing_id(),
                      Status(error::DATA_LOSS,
                             base::StrCat(
                                 {"Generation id mismatch, expected=",
                                  base::NumberToString(generation_id_.value()),
                                  " actual=",
                                  base::NumberToString(
                                      sequence_information.generation_id())})),
                      std::move(processed_cb));
        return;
      }
      if (!generation_id_.has_value()) {
        generation_id_ = sequence_information.generation_id();
      }

      // Verify digest and its match.
      {
        std::string serialized_record;
        wrapped_record.record().SerializeToString(&serialized_record);
        const auto record_digest = crypto::SHA256HashString(serialized_record);
        DCHECK_EQ(record_digest.size(), crypto::kSHA256Length);
        if (record_digest != wrapped_record.record_digest()) {
          sequence_bound_upload_
              .AsyncCall(&SequenceBoundUpload::DoUploadRecordFailure)
              .WithArgs(uploader_id_, sequence_information.sequencing_id(),
                        Status(error::DATA_LOSS, "Record digest mismatch"),
                        std::move(processed_cb));
          return;
        }
        // Store record digest for the next record in sequence to
        // verify.
        last_record_digest_map_->emplace(
            std::make_pair(sequence_information.sequencing_id(),
                           sequence_information.generation_id()),
            record_digest);
        // If last record digest is present, match it and validate,
        // unless previous record was a gap.
        if (wrapped_record.has_last_record_digest()) {
          auto it = last_record_digest_map_->find(
              std::make_pair(sequence_information.sequencing_id() - 1,
                             sequence_information.generation_id()));
          if (it == last_record_digest_map_->end() ||
              (it->second.has_value() &&
               it->second.value() != wrapped_record.last_record_digest())) {
            sequence_bound_upload_
                .AsyncCall(&SequenceBoundUpload::DoUploadRecordFailure)
                .WithArgs(
                    uploader_id_, sequence_information.sequencing_id(),
                    Status(error::DATA_LOSS, "Last record digest mismatch"),
                    std::move(processed_cb));
            return;
          }
        }
      }

      sequence_bound_upload_.AsyncCall(&SequenceBoundUpload::DoUploadRecord)
          .WithArgs(uploader_id_, sequence_information.sequencing_id(),
                    wrapped_record.record().data(), std::move(processed_cb));
    }

    SEQUENCE_CHECKER(test_uploader_checker_);

    // Unique ID of the uploader - even if the uploader is allocated
    // on the same address as an earlier one (already released),
    // it will get a new id and thus will ensure the expectations
    // match the expected uploader.
    const int64_t uploader_id_;

    absl::optional<int64_t> generation_id_;
    const raw_ptr<LastRecordDigestMap> last_record_digest_map_;

    const raw_ptr<const MockUpload> mock_upload_;

    base::SequenceBound<SequenceBoundUpload> sequence_bound_upload_;

    Sequence test_encounter_sequence_;
    Sequence test_upload_sequence_;
  };

  void CreateTestStorageQueueOrDie(const QueueOptions& options) {
    ASSERT_FALSE(storage_queue_) << "TestStorageQueue already assigned";
    auto storage_queue_result = CreateTestStorageQueue(options);
    ASSERT_OK(storage_queue_result)
        << "Failed to create TestStorageQueue, error="
        << storage_queue_result.status();
    storage_queue_ = std::move(storage_queue_result.ValueOrDie());
  }

  void CreateTestEncryptionModuleOrDie() {
    test_encryption_module_ =
        base::MakeRefCounted<test::TestEncryptionModule>();
    test::TestEvent<Status> key_update_event;
    test_encryption_module_->UpdateAsymmetricKey("DUMMY KEY", 0,
                                                 key_update_event.cb());
    const auto status = key_update_event.result();
    ASSERT_OK(status) << status;
  }

  // Tries to create a new storage queue by building the test encryption module
  // and returns the corresponding result of the operation.
  StatusOr<scoped_refptr<StorageQueue>> CreateTestStorageQueue(
      const QueueOptions& options) {
    CreateTestEncryptionModuleOrDie();
    test::TestEvent<StatusOr<scoped_refptr<StorageQueue>>>
        storage_queue_create_event;
    StorageQueue::Create(
        options,
        base::BindRepeating(&StorageQueueTest::AsyncStartMockUploader,
                            base::Unretained(this)),
        test_encryption_module_,
        CompressionModule::Create(kCompressionThreshold, kCompressionType),
        storage_queue_create_event.cb());

    return storage_queue_create_event.result();
  }

  void ResetTestStorageQueue() {
    // Let everything ongoing to finish.
    task_environment_.RunUntilIdle();
    storage_queue_.reset();
    // StorageQueue is destructed on a thread,
    // so we need to wait for it to destruct.
    task_environment_.RunUntilIdle();
  }

  void InjectFailures(const test::StorageQueueOperationKind operation_kind,
                      std::initializer_list<int64_t> sequencing_ids) {
    storage_queue_->TestInjectErrorsForOperation(operation_kind,
                                                 sequencing_ids);
  }

  QueueOptions BuildStorageQueueOptionsImmediate(
      base::TimeDelta upload_retry_delay = base::Seconds(1)) const {
    return QueueOptions(options_)
        .set_subdirectory(FILE_PATH_LITERAL("D1"))
        .set_file_prefix(FILE_PATH_LITERAL("F0001"))
        .set_upload_retry_delay(upload_retry_delay)
        .set_max_single_file_size(testing::get<0>(GetParam()));
  }

  QueueOptions BuildStorageQueueOptionsPeriodic(
      base::TimeDelta upload_period = base::Seconds(1)) const {
    return BuildStorageQueueOptionsImmediate().set_upload_period(upload_period);
  }

  QueueOptions BuildStorageQueueOptionsOnlyManual() const {
    return BuildStorageQueueOptionsPeriodic(base::TimeDelta::Max());
  }

  void AsyncStartMockUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](UploaderInterface::UploadReason reason,
               UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
               StorageQueueTest* self) {
              auto result = self->set_mock_uploader_expectations_.Call(reason);
              LOG(ERROR) << "Upload reason="
                         << UploaderInterface::ReasonToString(reason) << " "
                         << result.status();
              if (!result.ok()) {
                std::move(start_uploader_cb).Run(result.status());
                return;
              }
              auto uploader = std::move(result.ValueOrDie());
              std::move(start_uploader_cb).Run(std::move(uploader));
            },
            reason, std::move(start_uploader_cb), base::Unretained(this)));
  }

  Status WriteString(base::StringPiece data) {
    Record record;
    record.set_data(std::string(data));
    record.set_destination(UPLOAD_EVENTS);
    if (!dm_token_.empty()) {
      record.set_dm_token(dm_token_);
    }

    return WriteRecord(std::move(record));
  }

  Status WriteRecord(const Record record) {
    EXPECT_TRUE(storage_queue_) << "StorageQueue not created yet";
    test::TestEvent<Status> write_event;
    LOG(ERROR) << "Write data='" << record.data() << "'";
    storage_queue_->Write(std::move(record), write_event.cb());
    return write_event.result();
  }

  void WriteStringOrDie(base::StringPiece data) {
    const Status write_result = WriteString(data);
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(absl::optional<std::int64_t> sequencing_id,
                    bool force = false) {
    test::TestEvent<Status> c;
    LOG(ERROR) << "Confirm force=" << force << " seq="
               << (sequencing_id.has_value()
                       ? base::NumberToString(sequencing_id.value())
                       : "null");
    storage_queue_->Confirm(sequencing_id, force, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  std::string dm_token_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Sequenced task runner where all EXPECTs will happen.
  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_{
      base::SequencedTaskRunnerHandle::Get()};

  base::ScopedTempDir location_;
  StorageOptions options_;
  scoped_refptr<test::TestEncryptionModule> test_encryption_module_;
  scoped_refptr<StorageQueue> storage_queue_;

  // Test-wide global mapping of <generation id, sequencing id> to record
  // digest. Serves all TestUploaders created by test fixture.
  TestUploader::LastRecordDigestMap last_record_digest_map_;

  const ::testing::NiceMock<const MockUpload> mock_upload_;

  ::testing::MockFunction<StatusOr<std::unique_ptr<TestUploader>>(
      UploaderInterface::UploadReason /*reason*/)>
      set_mock_uploader_expectations_;
};

constexpr std::array<const char*, 3> kData = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> kMoreData = {"More1111", "More222",
                                                  "More33"};

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndReopen) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenAndWriteMore) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndUploadWithFailures) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Inject simulated failures.
  InjectFailures(test::StorageQueueOperationKind::kReadBlock, {1});

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .RequiredGap(1, 1)
            .Possible(2, kData[2])  // Depending on records binpacking
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  // Set uploader expectations.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest,
       WriteIntoNewStorageQueueReopenWithMissingMetadataWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  ResetTestStorageQueue();

  // Delete all metadata files.
  EnsureDeletingFiles(options.directory(),
                      /*recursive=*/false, base::FileEnumerator::FILES,
                      base::StrCat({METADATA_NAME, FILE_PATH_LITERAL(".*")}));

  // Reopen, starting a new generation.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations. Previous data is all lost.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(
    StorageQueueTest,
    WriteIntoNewStorageQueueReopenWithMissingLastMetadataWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  ResetTestStorageQueue();

  // Delete the last metadata file.
  {  // scoping this block so that dir_enum is not used later.
    const auto last_metadata_file_pattern =
        base::StrCat({METADATA_NAME, FILE_PATH_LITERAL(".2")});
    base::FileEnumerator dir_enum(options.directory(),
                                  /*recursive=*/false,
                                  base::FileEnumerator::FILES,
                                  last_metadata_file_pattern);
    base::FilePath full_name = dir_enum.Next();
    ASSERT_FALSE(full_name.empty())
        << "No file matches " << last_metadata_file_pattern;
    ASSERT_TRUE(dir_enum.Next().empty())
        << full_name << " is not the last metadata file in "
        << options.directory();
    ASSERT_TRUE(base::DeleteFile(full_name))
        << "Failed to delete " << full_name;
  }

  // Reopen, starting a new generation.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations. Previous data is all lost.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest,
       WriteIntoNewStorageQueueReopenWithMissingDataWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  ResetTestStorageQueue();

  // Reopen with the same generation and sequencing information.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Delete the data files *.generation.0
  EnsureDeletingFiles(
      options.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({options.file_prefix(), FILE_PATH_LITERAL(".*.0")}));

  // Write more data.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations. Previous data is all lost.
  // The expected results depend on the test configuration.
  test::TestCallbackAutoWaiter waiter;
  switch (options.max_single_file_size()) {
    case 1:  // single record in file - deletion killed the first record
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(&waiter, this)
                    .PossibleGap(0, 1)
                    .Required(1, kData[1])
                    .Required(2, kData[2])
                    .Required(3, kMoreData[0])
                    .Required(4, kMoreData[1])
                    .Required(5, kMoreData[2])
                    .Complete();
              }))
          .RetiresOnSaturation();
      break;
    case 256:  // two records in file - deletion killed the first two records.
               // Can bring gap of 2 records or 2 gaps 1 record each.
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(&waiter, this)
                    .PossibleGap(0, 1)
                    .PossibleGap(1, 1)
                    .PossibleGap(0, 2)
                    .Failure(2, Status(error::DATA_LOSS,
                                       "Last record digest mismatch"))
                    .Required(3, kMoreData[0])
                    .Required(4, kMoreData[1])
                    .Required(5, kMoreData[2])
                    .Complete();
              }))
          .RetiresOnSaturation();
      break;
    default:  // Unlimited file size - deletion above killed all the data. Can
              // bring gap of 1-6 records.
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(&waiter, this)
                    .PossibleGap(0, 1)
                    .PossibleGap(0, 2)
                    .PossibleGap(0, 3)
                    .PossibleGap(0, 4)
                    .PossibleGap(0, 5)
                    .PossibleGap(0, 6)
                    .Complete();
              }))
          .RetiresOnSaturation();
  }

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndFlush) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::MANUAL)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenWriteMoreAndFlush) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::MANUAL)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, ValidateVariousRecordSizes) {
  std::vector<std::string> data;
  for (size_t i = 16; i < 16 + 16; ++i) {
    data.emplace_back(i, 'R');
  }
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  for (const auto& record : data) {
    WriteStringOrDie(record);
  }

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::MANUAL)))
      .WillOnce(Invoke(
          [&waiter, &data, this](UploaderInterface::UploadReason reason) {
            TestUploader::SetUp uploader_setup(&waiter, this);
            for (size_t i = 0; i < data.size(); ++i) {
              uploader_setup.Required(i, data[i]);
            }
            return uploader_setup.Complete();
          }))
      .RetiresOnSaturation();

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }
  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*sequencing_id=*/1);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*sequencing_id=*/2);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyUploadWithConfirmationsAndReopen) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*sequencing_id=*/1);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  ResetTestStorageQueue();
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Possible(0, kData[0])
                  .Possible(1, kData[1])
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*sequencing_id=*/2);

  {
    test::TestCallbackAutoWaiter waiter;
    // Set uploader expectations.
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest,
       WriteAndRepeatedlyUploadWithConfirmationsAndReopenWithFailures) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*sequencing_id=*/1);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  ResetTestStorageQueue();
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Inject simulated failures.
  InjectFailures(test::StorageQueueOperationKind::kReadBlock, {4, 5});

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Possible(0, kData[0])
                  .Possible(1, kData[1])
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  // Gap may be 2 records at once or 2 gaps 1 record each.
                  .PossibleGap(4, 2)
                  .PossibleGap(4, 1)
                  .PossibleGap(5, 1)
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*sequencing_id=*/2);

  // Reset simulated failures.
  InjectFailures(test::StorageQueueOperationKind::kReadBlock, {});

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Possible(1, kData[1])
                  .Possible(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Possible(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[2]);
  }
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the data that may be eliminated by Confirmation as
  // |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[2]);
  }

  // Confirm #1, removing data #0 and #1
  ConfirmOrDie(/*sequencing_id=*/1);

  // Add more data to verify that #2 and new data are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kMoreData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kMoreData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kMoreData[2]);
  }
}

TEST_P(StorageQueueTest, WriteAndImmediateUploadWithFailure) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Write a record as Immediate, initiating an upload which fails
  // and then restarts.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(Invoke([](UploaderInterface::UploadReason reason) {
          return Status(error::UNAVAILABLE, "Test uploader unavailable");
        }))
        .RetiresOnSaturation();
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::FAILURE_RETRY)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);  // Immediately uploads and fails.

    // Let it retry upload and verify.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest, WriteAndImmediateUploadWithoutConfirmation) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Write a record as Immediate, initiating an upload which fails
  // and then restarts.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);  // Immediately uploads and does not confirm.
  }

  // Let it retry upload and verify.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::INCOMPLETE_RETRY)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm 0 and make sure no retry happens (since everything is confirmed).
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::INCOMPLETE_RETRY)))
      .Times(0);

  ConfirmOrDie(/*sequencing_id=*/0);
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteEncryptFailure) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  DCHECK(test_encryption_module_);
  EXPECT_CALL(*test_encryption_module_, EncryptRecordImpl(_, _))
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            std::move(cb).Run(Status(error::UNKNOWN, "Failing for tests"));
          })));
  const Status result = WriteString("TEST_MESSAGE");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

TEST_P(StorageQueueTest, ForceConfirm) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, possibly removing records #0 and #1
  ConfirmOrDie(/*sequencing_id=*/1);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Now force confirm the very beginning and forward time again.
  ConfirmOrDie(/*sequencing_id=*/absl::nullopt, /*force=*/true);

  {
    // Set uploader expectations.
    // #0 and #1 could be returned as Gaps
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .RequiredSeqId(0)
                  .RequiredSeqId(1)
                  .RequiredSeqId(2)
                  // 0-2 must have been encountered, but actual contents
                  // can be different:
                  .Possible(0, kData[0])
                  .PossibleGap(0, 1)
                  .PossibleGap(0, 2)
                  .Possible(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Force confirm #0 and forward time again.
  ConfirmOrDie(/*sequencing_id=*/0, /*force=*/true);

  {
    // Set uploader expectations.
    // #0 and #1 could be returned as Gaps
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(&waiter, this)
                  .RequiredSeqId(1)
                  .RequiredSeqId(2)
                  // 0-2 must have been encountered, but actual contents
                  // can be different:
                  .PossibleGap(1, 1)
                  .Possible(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest, WriteInvalidRecord) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  const Record invalid_record;
  Status write_result = WriteRecord(std::move(invalid_record));
  EXPECT_FALSE(write_result.ok());
  EXPECT_EQ(write_result.error_code(), error::FAILED_PRECONDITION);
}

TEST_P(StorageQueueTest, WriteRecordWithNoData) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  Record record;
  record.set_destination(UPLOAD_EVENTS);
  Status write_result = WriteRecord(std::move(record));
  EXPECT_OK(write_result);
}

TEST_P(StorageQueueTest, WriteRecordWithWriteMetadataFailures) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  InjectFailures(test::StorageQueueOperationKind::kWriteMetadata, {0});
  Status write_result = WriteString(kData[0]);
  EXPECT_FALSE(write_result.ok());
  EXPECT_EQ(write_result.error_code(), error::INTERNAL);
}

TEST_P(StorageQueueTest, WriteRecordWithWriteBlockFailures) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  InjectFailures(test::StorageQueueOperationKind::kWriteBlock, {0});
  Status write_result = WriteString(kData[0]);
  EXPECT_FALSE(write_result.ok());
  EXPECT_EQ(write_result.error_code(), error::INTERNAL);
}

TEST_P(StorageQueueTest, WriteRecordWithInvalidFilePrefix) {
  QueueOptions options = BuildStorageQueueOptionsPeriodic();
  options.set_file_prefix(kInvalidFilePrefix);
  CreateTestStorageQueueOrDie(options);
  Status write_result = WriteString(kData[0]);
  EXPECT_FALSE(write_result.ok());
  EXPECT_EQ(write_result.error_code(), error::ALREADY_EXISTS);
}

TEST_P(StorageQueueTest, CreateStorageQueueInvalidOptionsPath) {
  options_.set_directory(base::FilePath(kInvalidDirectoryPath));
  StatusOr<scoped_refptr<StorageQueue>> queue_result =
      CreateTestStorageQueue(BuildStorageQueueOptionsPeriodic());
  EXPECT_FALSE(queue_result.ok());
  EXPECT_EQ(queue_result.status().error_code(), error::UNAVAILABLE);
}

TEST_P(StorageQueueTest, WriteRecordWithInsufficientDiskSpace) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Update total disk space and reset after running the write operation so it
  // does not affect other tests
  const auto original_disk_space = options_.disk_space_resource()->GetTotal();
  options_.disk_space_resource()->Test_SetTotal(0);
  Status write_result = WriteString(kData[0]);
  options_.disk_space_resource()->Test_SetTotal(original_disk_space);
  EXPECT_FALSE(write_result.ok());
  EXPECT_EQ(write_result.error_code(), error::RESOURCE_EXHAUSTED);
}

TEST_P(StorageQueueTest, WriteRecordWithInsufficientMemory) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Update total memory and reset after running the write operation so it does
  // not affect other tests
  const auto original_total_memory = options_.memory_resource()->GetTotal();
  options_.memory_resource()->Test_SetTotal(0);
  Status write_result = WriteString(kData[0]);
  options_.memory_resource()->Test_SetTotal(original_total_memory);
  EXPECT_FALSE(write_result.ok());
  EXPECT_EQ(write_result.error_code(), error::RESOURCE_EXHAUSTED);
}

TEST_P(StorageQueueTest, UploadWithInsufficientMemory) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);

  const auto original_total_memory = options_.memory_resource()->GetTotal();

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        // Update total memory to a low amount.
        options_.memory_resource()->Test_SetTotal(100u);
        return TestUploader::SetUp(&waiter, this)
            .Complete(Status(error::RESOURCE_EXHAUSTED,
                             "Insufficient memory for upload"));
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::FAILURE_RETRY)))
      .WillOnce(Invoke([&waiter, &original_total_memory,
                        this](UploaderInterface::UploadReason reason) {
        // Reset after running upload so it does not affect other tests.
        options_.memory_resource()->Test_SetTotal(original_total_memory);
        return TestUploader::SetUp(&waiter, this)
            .Required(0, kData[0])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload which will experience insufficient memory.
  task_environment_.FastForwardBy(base::Seconds(1));
  // Trigger another upload resetting the memory resource.
  task_environment_.FastForwardBy(base::Seconds(1));
}

INSTANTIATE_TEST_SUITE_P(
    VaryingFileSize,
    StorageQueueTest,
    testing::Combine(testing::Values(128 * 1024LL * 1024LL,
                                     256 /* two records in file */,
                                     1 /* single record in file */),
                     testing::Values("DM TOKEN", "")));

}  // namespace
}  // namespace reporting