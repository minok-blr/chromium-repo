// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using RemoteHostContactedInfo =
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo;
using RemoteHostInfo =
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo;

constexpr const char* kExtensionId[] = {"crx-0", "crx-1"};
const char* host_urls[] = {"http://www.9oo91e.qjz9zk", "http://www.y0u1ub3.qjz9zk"};

class RemoteHostContactedSignalProcessorTest : public ::testing::Test {
 protected:
  RemoteHostContactedSignalProcessorTest() = default;

  RemoteHostContactedSignalProcessor processor_;
};

TEST_F(RemoteHostContactedSignalProcessorTest,
       EmptyProcessorMapWithNoDataStored) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(RemoteHostContactedSignalProcessorTest,
       StoresDataAfterProcessingSignal) {
  auto signal = RemoteHostContactedSignal(kExtensionId[0], GURL(host_urls[0]));
  processor_.ProcessSignal(signal);

  // Verify that processor now has some data to report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Verify that there is signal info only for the correct extension id.
  EXPECT_TRUE(processor_.GetSignalInfoForReport(kExtensionId[0]));
  EXPECT_FALSE(processor_.GetSignalInfoForReport(kExtensionId[1]));
}

// Test contacted_count and mapping functionalities.
TEST_F(RemoteHostContactedSignalProcessorTest, ReportsSignalInfoCorrectly) {
  // Process 3 signals for the first extension, each corresponding to the
  // web request sent to the first test url.
  for (int i = 0; i < 3; i++) {
    auto signal =
        RemoteHostContactedSignal(kExtensionId[0], GURL(host_urls[0]));
    processor_.ProcessSignal(signal);
  }

  // Process 3 signals for the second extension. Two signal corresponds to the
  // web request sent to the first url, the third to the web request
  // sent to the second url.
  for (int i = 0; i < 2; i++) {
    auto signal =
        RemoteHostContactedSignal(kExtensionId[1], GURL(host_urls[0]));
    processor_.ProcessSignal(signal);
  }
  {
    auto signal =
        RemoteHostContactedSignal(kExtensionId[1], GURL(host_urls[1]));
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info for first extension.
  std::unique_ptr<SignalInfo> extension_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(extension_0_signal_info, nullptr);

  // Verify that processor still has some data to report (for the second
  // extension).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for the second extension.
  std::unique_ptr<SignalInfo> extension_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[1]);
  ASSERT_NE(extension_1_signal_info, nullptr);

  // Verify that processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents for first extension.
  {
    const RemoteHostContactedInfo& remote_host_contacted_info =
        extension_0_signal_info->remote_host_contacted_info();

    // Verify data stored : only 1 url (contacted 3 times).
    ASSERT_EQ(remote_host_contacted_info.remote_host_size(), 1);
    const RemoteHostInfo& remote_host_info =
        remote_host_contacted_info.remote_host(0);
    EXPECT_EQ(remote_host_info.contact_count(), static_cast<uint32_t>(3));
  }

  // Verify signal info contents for second extension.
  {
    const RemoteHostContactedInfo& remote_host_contacted_info =
        extension_1_signal_info->remote_host_contacted_info();

    // Verify data stored : 2 URLs (2 contacted counts for 1st, 1 for the 2nd).
    ASSERT_EQ(remote_host_contacted_info.remote_host_size(), 2);
    {
      const RemoteHostInfo& remote_host_info =
          remote_host_contacted_info.remote_host(0);
      EXPECT_EQ(remote_host_info.contact_count(), static_cast<uint32_t>(2));
    }
    {
      const RemoteHostInfo& remote_host_info =
          remote_host_contacted_info.remote_host(1);
      EXPECT_EQ(remote_host_info.contact_count(), static_cast<uint32_t>(1));
    }
  }
}

}  // namespace

}  // namespace safe_browsing
