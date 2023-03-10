// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"
#include "components/segmentation_platform/internal/config_parser.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/query_tiles/switches.h"
#include "components/segmentation_platform/embedder/default_model/price_tracking_action_model.h"
#include "components/segmentation_platform/embedder/default_model/query_tiles_model.h"
#include "components/segmentation_platform/embedder/input_delegate/price_tracking_input_delegate.h"
#endif

namespace segmentation_platform {

using proto::SegmentId;

namespace {

constexpr char kDefaultModelEnabledParam[] = "enable_default_model";

// Default TTL for segment selection and unknown selection:

constexpr int kDummyFeatureSelectionTTLDays = 1;

constexpr int kChromeLowUserEngagementSelectionTTLDays = 7;

constexpr int kFeedUserSegmentSelectionTTLDays = 14;
constexpr int kFeedUserSegmentUnknownSelectionTTLDays = 14;

#if BUILDFLAG(IS_ANDROID)

constexpr int kAdaptiveToolbarDefaultSelectionTTLDays = 56;

constexpr int kChromeStartDefaultSelectionTTLDays = 30;
constexpr int kChromeStartDefaultUnknownTTLDays = 7;
// See
// https://source.ch40m1um.qjz9zk/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/query_tiles/QueryTileUtils.java
const char kNumDaysKeepShowingQueryTiles[] =
    "num_days_keep_showing_query_tiles";
const char kNumDaysMVCkicksBelowThreshold[] =
    "num_days_mv_clicks_below_threshold";

// DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES
constexpr int kQueryTilesDefaultSelectionTTLDays = 28;
// DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD
constexpr int kQueryTilesDefaultUnknownTTLDays = 7;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<Config> GetConfigForAdaptiveToolbar() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAdaptiveToolbarSegmentationKey;
  config->segmentation_uma_name = kAdaptiveToolbarUmaName;

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2,
      "segment_selection_ttl_days", kAdaptiveToolbarDefaultSelectionTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  // Do not set unknown TTL so that the platform ignores unknown results.

  // A hardcoded list of segment IDs known to the segmentation platform.
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);

  return config;
}
#endif

std::unique_ptr<Config> GetConfigForDummyFeature() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kDummySegmentationKey;
  config->segmentation_uma_name = kDummyFeatureUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY);
  config->segment_selection_ttl = base::Days(kDummyFeatureSelectionTTLDays);
  config->unknown_selection_ttl = base::Days(kDummyFeatureSelectionTTLDays);
  return config;
}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<ModelProvider> GetChromeStartAndroidModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          chrome::android::kStartSurfaceAndroid, kDefaultModelEnabledParam,
          false)) {
    return nullptr;
  }
  return std::make_unique<ChromeStartModel>();
}

std::unique_ptr<Config> GetConfigForChromeStartAndroid() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeStartAndroidSegmentationKey;
  config->segmentation_uma_name = kChromeStartAndroidUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
      GetChromeStartAndroidModel());

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid, "segment_selection_ttl_days",
      kChromeStartDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid,
      "segment_unknown_selection_ttl_days", kChromeStartDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);

  return config;
}

std::unique_ptr<ModelProvider> GetQueryTilesDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          query_tiles::features::kQueryTilesSegmentation,
          kDefaultModelEnabledParam, false)) {
    return nullptr;
  }
  return std::make_unique<QueryTilesModel>();
}

std::unique_ptr<Config> GetConfigForQueryTiles() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kQueryTilesSegmentationKey;
  config->segmentation_uma_name = kQueryTilesUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES,
                       GetQueryTilesDefaultModel());

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysKeepShowingQueryTiles, kQueryTilesDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysMVCkicksBelowThreshold, kQueryTilesDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);
  return config;
}

bool IsEnabledContextualPageActions() {
  if (!base::FeatureList::IsEnabled(features::kContextualPageActions))
    return false;

  return base::FeatureList::IsEnabled(
             features::kContextualPageActionPriceTracking) &&
         base::FeatureList::IsEnabled(commerce::kShoppingList);
}

std::unique_ptr<Config> GetConfigForContextualPageActions(
    content::BrowserContext* context) {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kContextualPageActionsKey;
  config->segmentation_uma_name = kContextualPageActionsUmaName;
  if (base::FeatureList::IsEnabled(
          features::kContextualPageActionPriceTracking) &&
      base::FeatureList::IsEnabled(commerce::kShoppingList)) {
    config->AddSegmentId(
        SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
        std::make_unique<PriceTrackingActionModel>());

    auto shopping_service_getter = base::BindRepeating(
        commerce::ShoppingServiceFactory::GetForBrowserContextIfExists,
        context);
    auto price_tracking_input_delegate =
        std::make_unique<processing::PriceTrackingInputDelegate>(
            shopping_service_getter);
    config
        ->input_delegates[proto::CustomInput_FillPolicy_PRICE_TRACKING_HINTS] =
        std::move(price_tracking_input_delegate);
  }
  config->on_demand_execution = true;
  return config;
}

#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<ModelProvider> GetLowEngagementDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformLowEngagementFeature,
          kDefaultModelEnabledParam, true)) {
    return nullptr;
  }
  return std::make_unique<LowUserEngagementModel>();
}

bool IsLowEngagementFeatureEnabled() {
  // TODO(ssid): Remove this extra feature and change feature guide to use the
  // segmentation defined feature.
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          feature_guide::features::kSegmentationModelLowEngagedUsers)) {
    return true;
  }
#endif
  return base::FeatureList::IsEnabled(
      features::kSegmentationPlatformLowEngagementFeature);
}

std::unique_ptr<Config> GetConfigForChromeLowUserEngagement() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeLowUserEngagementSegmentationKey;
  config->segmentation_uma_name = kChromeLowUserEngagementUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT,
      GetLowEngagementDefaultModel());

#if BUILDFLAG(IS_ANDROID)
  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      feature_guide::features::kSegmentationModelLowEngagedUsers,
      "segment_selection_ttl_days", kChromeLowUserEngagementSelectionTTLDays);
#else
  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kSegmentationPlatformLowEngagementFeature,
      "segment_selection_ttl_days", kChromeLowUserEngagementSelectionTTLDays);
#endif

  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(segment_selection_ttl_days);
  return config;
}

std::unique_ptr<ModelProvider> GetFeedUserSegmentDefautlModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformFeedSegmentFeature,
          kDefaultModelEnabledParam, true)) {
    return nullptr;
  }
  return std::make_unique<FeedUserSegment>();
}

std::unique_ptr<Config> GetConfigForFeedSegments() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kFeedUserSegmentationKey;
  config->segmentation_uma_name = kFeedUserSegmentUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER,
                       GetFeedUserSegmentDefautlModel());
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          "segment_selection_ttl_days", kFeedUserSegmentSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          "unknown_selection_ttl_days",
          kFeedUserSegmentUnknownSelectionTTLDays));
  return config;
}

void AppendConfigsFromExperiments(
    std::vector<std::unique_ptr<Config>>& out_configs) {
  // TODO(crbug.com/1346389): Add logic to find segmentation param from field
  // trials.
  std::vector<std::string> params;
  for (const std::string& param : params) {
    auto config = ParseConfigFromString(param);
    if (config) {
      out_configs.push_back(std::move(config));
    }
  }
}

}  // namespace

std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig(
    content::BrowserContext* context) {
  std::vector<std::unique_ptr<Config>> configs;
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformDummyFeature)) {
    configs.emplace_back(GetConfigForDummyFeature());
  }
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2)) {
    configs.emplace_back(GetConfigForAdaptiveToolbar());
  }
  if (IsEnabledContextualPageActions()) {
    configs.emplace_back(GetConfigForContextualPageActions(context));
  }
  if (IsStartSurfaceBehaviouralTargetingEnabled()) {
    configs.emplace_back(GetConfigForChromeStartAndroid());
  }
  if (base::FeatureList::IsEnabled(
          query_tiles::features::kQueryTilesSegmentation)) {
    configs.emplace_back(GetConfigForQueryTiles());
  }
#endif
  if (IsLowEngagementFeatureEnabled()) {
    configs.emplace_back(GetConfigForChromeLowUserEngagement());
  }

  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformFeedSegmentFeature)) {
    configs.emplace_back(GetConfigForFeedSegments());
  }

  AppendConfigsFromExperiments(configs);
  return configs;
}

FieldTrialRegisterImpl::FieldTrialRegisterImpl() = default;
FieldTrialRegisterImpl::~FieldTrialRegisterImpl() = default;

void FieldTrialRegisterImpl::RegisterFieldTrial(base::StringPiece trial_name,
                                                base::StringPiece group_name) {
  // The register method is called early in startup once the platform is
  // initialized. So, in most cases the client will register the field trial
  // before uploading the first UMA log of the current session. We do not want
  // to annotate logs from the previous session. (These comes in two types:
  // histograms persisted from the previous session or stability information
  // about the previous session.) Groups are not stable across sessions; we
  // don't know if the current segmentation applies to the previous session.
  // Incidentally, the platform records metrics to track the movement between
  // groups.
  // TODO(ssid): Move to a MetricsProvider approach to fill the groups so we are
  // able to track how often we miss the first session log due to delays in
  // platform initialization.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

void FieldTrialRegisterImpl::RegisterSubsegmentFieldTrialIfNeeded(
    base::StringPiece trial_name,
    SegmentId segment_id,
    int subsegment_rank) {
  absl::optional<std::string> group_name;
  // TODO(ssid): Make GetSubsegmentName as a ModelProvider API so that clients
  // can simply implement it instead of adding conditions here, once the
  // subsegment process is more stable.
  if (segment_id == SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER) {
    group_name = FeedUserSegment::GetSubsegmentName(subsegment_rank);
  }

  if (!group_name) {
    return;
  }
  RegisterFieldTrial(trial_name, *group_name);
}

}  // namespace segmentation_platform
