// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace crostini {

// static
CrostiniManager* CrostiniManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<CrostiniManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniManagerFactory* CrostiniManagerFactory::GetInstance() {
  static base::NoDestructor<CrostiniManagerFactory> factory;
  return factory.get();
}

CrostiniManagerFactory::CrostiniManagerFactory()
    : ProfileKeyedServiceFactory("CrostiniManager") {}

CrostiniManagerFactory::~CrostiniManagerFactory() = default;

KeyedService* CrostiniManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniManager(profile);
}

}  // namespace crostini