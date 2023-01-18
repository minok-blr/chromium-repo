// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_client_side_phishing_component_installer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/installer_policies/client_side_phishing_component_installer_policy.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/update_client/update_client.h"

using component_updater::ComponentUpdateService;

namespace component_updater {
namespace {

void LoadFromDisk(const base::FilePath& pb_path,
                  const base::FilePath& visual_tflite_model_path) {
}

void PopulateModelFromFiles(const base::FilePath& install_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadFromDisk,
                     install_dir.Append(kClientModelBinaryPbFileName),
                     install_dir.Append(kVisualTfLiteModelFileName)));
}

update_client::InstallerAttributes GetInstallerAttributes() {
  update_client::InstallerAttributes attributes;

  return attributes;
}

}  // namespace

void RegisterClientSidePhishingComponent(ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ClientSidePhishingComponentInstallerPolicy>(
          base::BindRepeating(&PopulateModelFromFiles),
          base::BindRepeating(&GetInstallerAttributes)));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
