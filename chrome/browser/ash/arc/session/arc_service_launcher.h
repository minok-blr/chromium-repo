// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_SERVICE_LAUNCHER_H_

#include <memory>

#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

class Profile;


namespace chromeos {
class SchedulerConfigurationManagerBase;
}

namespace arc {

class ArcDemoModePreferenceHandler;
class ArcDiskSpaceMonitor;
class ArcIconCacheDelegateProvider;
class ArcPlayStoreEnabledPreferenceHandler;
class ArcServiceManager;
class ArcSessionManager;

// Detects ARC availability and launches ARC bridge service.
class ArcServiceLauncher {
 public:
  // |scheduler_configuration_manager| must outlive |this| object.
  explicit ArcServiceLauncher(chromeos::SchedulerConfigurationManagerBase*
                                  scheduler_configuration_manager);

  ArcServiceLauncher(const ArcServiceLauncher&) = delete;
  ArcServiceLauncher& operator=(const ArcServiceLauncher&) = delete;

  ~ArcServiceLauncher();

  // Returns a global instance.
  static ArcServiceLauncher* Get();

  // Must be called early in startup.
  void Initialize();

  // Called just before most of BrowserContextKeyedService instance creation.
  // Set the given |profile| to ArcSessionManager, if the profile is allowed
  // to use ARC.
  void MaybeSetProfile(Profile* profile);

  // Called when the main profile is initialized after user logs in.
  void OnPrimaryUserProfilePrepared(Profile* profile);

  // Called after the main MessageLoop stops, and before the Profile is
  // destroyed.
  void Shutdown();

  // Resets internal state for testing. Specifically this needs to be
  // called if other profile needs to be used in the tests. In that case,
  // following this call, MaybeSetProfile() and
  // OnPrimaryUserProfilePrepared() should be called.
  void ResetForTesting();

 private:
#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  // Callback for when the CdmFactoryDaemon D-Bus service is available, also
  // used to trigger expanding the property files if a timeout occurs after we
  // detect TPM ownership.  The |from_timeout| parameter indicates if the call
  // came from the timeout case or from the D-Bus service availability case.
  void OnCdmFactoryDaemonAvailable(bool from_timeout,
                                   bool is_service_available);

  // Delayed callback for when we should check the TPM status.
  void OnCheckTpmStatus();

  // Callback used for checking if the TPM is owned yet.
  void OnGetTpmStatus(
      const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply);

  // For tracking whether or not we have invoked property file expansion on the
  // session manager since this can happen via a timeout or callback.
  bool expanded_property_files_ = false;
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcPlayStoreEnabledPreferenceHandler>
      arc_play_store_enabled_preference_handler_;
  std::unique_ptr<ArcDemoModePreferenceHandler>
      arc_demo_mode_preference_handler_;
  std::unique_ptr<ArcDiskSpaceMonitor> arc_disk_space_monitor_;
  std::unique_ptr<ArcIconCacheDelegateProvider>
      arc_icon_cache_delegate_provider_;
  // |scheduler_configuration_manager_| outlives |this|.
  chromeos::SchedulerConfigurationManagerBase* const
      scheduler_configuration_manager_;

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  base::WeakPtrFactory<ArcServiceLauncher> weak_factory_{this};
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_SERVICE_LAUNCHER_H_