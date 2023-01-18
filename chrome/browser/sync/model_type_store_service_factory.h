// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace syncer {
class ModelTypeStoreService;
}  // namespace syncer

class ModelTypeStoreServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static syncer::ModelTypeStoreService* GetForProfile(Profile* profile);
  static ModelTypeStoreServiceFactory* GetInstance();

  ModelTypeStoreServiceFactory(const ModelTypeStoreServiceFactory&) = delete;
  ModelTypeStoreServiceFactory& operator=(const ModelTypeStoreServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<ModelTypeStoreServiceFactory>;

  ModelTypeStoreServiceFactory();
  ~ModelTypeStoreServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_