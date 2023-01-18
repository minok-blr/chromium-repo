// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/configure.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/operations/test_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;

}  // namespace

class FileSystemProviderOperationsConfigureTest : public testing::Test {
 protected:
  FileSystemProviderOperationsConfigureTest() {}
  ~FileSystemProviderOperationsConfigureTest() override {}

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, "" /* display_name */),
        base::FilePath(), false /* configurable */, true /* watchable */,
        extensions::SOURCE_FILE, IconSet());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsConfigureTest, Execute) {
  using extensions::api::file_system_provider::ConfigureRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Configure configure(NULL, file_system_info_,
                      base::BindOnce(&util::LogStatusCallback, &callback_log));
  configure.SetDispatchEventImplForTesting(
      base::BindRepeating(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                          base::Unretained(&dispatcher)));

  EXPECT_TRUE(configure.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnConfigureRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  ConfigureRequestedOptions options;
  ASSERT_TRUE(ConfigureRequestedOptions::Populate(*options_as_value, &options));
  EXPECT_EQ(kFileSystemId, options.file_system_id);
  EXPECT_EQ(kRequestId, options.request_id);
}

TEST_F(FileSystemProviderOperationsConfigureTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Configure configure(NULL, file_system_info_,
                      base::BindOnce(&util::LogStatusCallback, &callback_log));
  configure.SetDispatchEventImplForTesting(
      base::BindRepeating(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                          base::Unretained(&dispatcher)));

  EXPECT_FALSE(configure.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsConfigureTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Configure configure(NULL, file_system_info_,
                      base::BindOnce(&util::LogStatusCallback, &callback_log));
  configure.SetDispatchEventImplForTesting(
      base::BindRepeating(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                          base::Unretained(&dispatcher)));

  EXPECT_TRUE(configure.Execute(kRequestId));

  configure.OnSuccess(kRequestId, std::make_unique<RequestValue>(),
                      false /* has_more */);
  ASSERT_EQ(1u, callback_log.size());
  base::File::Error event_result = callback_log[0];
  EXPECT_EQ(base::File::FILE_OK, event_result);
}

TEST_F(FileSystemProviderOperationsConfigureTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Configure configure(NULL, file_system_info_,
                      base::BindOnce(&util::LogStatusCallback, &callback_log));
  configure.SetDispatchEventImplForTesting(
      base::BindRepeating(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                          base::Unretained(&dispatcher)));

  EXPECT_TRUE(configure.Execute(kRequestId));

  configure.OnError(kRequestId, std::make_unique<RequestValue>(),
                    base::File::FILE_ERROR_NOT_FOUND);
  ASSERT_EQ(1u, callback_log.size());
  base::File::Error event_result = callback_log[0];
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, event_result);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash