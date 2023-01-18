// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/domain_reliability/service_factory.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_switches.h"

namespace domain_reliability {

// Identifies Chrome as the source of Domain Reliability uploads it sends.
const char DomainReliabilityServiceFactory::kUploadReporterString[] = "chrome";

// static
bool DomainReliabilityServiceFactory::ShouldCreateService() {
  return false;
}

}  // namespace domain_reliability
