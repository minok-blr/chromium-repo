// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {
std::unique_ptr<views::WebView> CreateWebView(
    views::View* host,
    content::BrowserContext* browser_context) {
  auto web_view = std::make_unique<views::WebView>(browser_context);
  // Set a flex behavior for the WebView to always fill out the extra space in
  // the parent view. In the minimum case, it will scale down to 0.
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // Set background of webview to the same background as the toolbar. This is to
  // prevent personal color themes from showing in the side panel when
  // navigating to a new Lens results panel.
  web_view->SetBackground(views::CreateThemedSolidBackground(kColorToolbar));
  return web_view;
}

}  // namespace

namespace lens {
constexpr int kDefaultSidePanelHeaderHeight = 40;
constexpr gfx::Insets kLensLabelButtonMargins = gfx::Insets::VH(12, 0);
constexpr char kStaticLoadingScreenURL[] =
    "https://www.95tat1c.qjz9zk/lens/chrome/lens_side_panel_loading.html";

LensUnifiedSidePanelView::LensUnifiedSidePanelView(BrowserView* browser_view) {
  browser_view_ = browser_view;
  auto* browser_context = browser_view->GetProfile();
  // Align views vertically top to bottom.
  SetOrientation(views::LayoutOrientation::kVertical);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);

  // Stretch views to fill horizontal bounds.
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  loading_indicator_web_view_ =
      AddChildView(CreateWebView(this, browser_context));
  loading_indicator_web_view_->GetWebContents()->GetController().LoadURL(
      GURL(kStaticLoadingScreenURL), content::Referrer(),
      ui::PAGE_TRANSITION_FROM_API, std::string());
  web_view_ = AddChildView(CreateWebView(this, browser_context));
  separator_ = AddChildView(std::make_unique<views::Separator>());

  if (lens::features::GetEnableLensSidePanelFooter())
    CreateAndInstallFooter();

  SetContentVisible(false);
  auto* web_contents = web_view_->GetWebContents();
  web_contents->SetDelegate(this);
  Observe(web_contents);
}

content::WebContents* LensUnifiedSidePanelView::GetWebContents() {
  return web_view_->GetWebContents();
}

void LensUnifiedSidePanelView::LoadResultsInNewTab() {
  const GURL url = lens::CreateURLForNewTab(
      web_view_->GetWebContents()->GetLastCommittedURL());
  // If there is no payload parameter, we will have an empty URL. This means
  // we should return on empty and not close the side panel.
  if (url.is_empty())
    return;
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_TYPED,
                                /*is_renderer_initiated=*/false);
  browser_view_->browser()->OpenURL(params);
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.LoadResultsInNewTab"));
  browser_view_->side_panel_coordinator()->Close();
}

void LensUnifiedSidePanelView::LoadProgressChanged(double progress) {
  bool is_content_visible = progress == 1.0;
  SetContentVisible(is_content_visible);
  if (launch_button_ != nullptr && is_content_visible) {
    auto last_committed_url =
        web_view_->GetWebContents()->GetLastCommittedURL();
    launch_button_->SetEnabled(lens::IsValidLensResultUrl(last_committed_url));
  }
}

bool LensUnifiedSidePanelView::IsLaunchButtonEnabledForTesting() {
  return launch_button_ != nullptr && launch_button_->GetEnabled();
}

bool LensUnifiedSidePanelView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

void LensUnifiedSidePanelView::OpenUrl(const content::OpenURLParams& params) {
  side_panel_url_params_ = std::make_unique<content::OpenURLParams>(params);
  MaybeLoadURLWithParams();
}

void LensUnifiedSidePanelView::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  content::OpenURLParams params(url, referrer, disposition, transition,
                                renderer_initiated);
  // If the navigation is initiated by the renderer process, we must set an
  // initiator origin.
  if (renderer_initiated)
    params.initiator_origin = url::Origin::Create(url);

  browser_view_->browser()->OpenURL(params);
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.ResultLinkClick"));
}

void LensUnifiedSidePanelView::CreateAndInstallFooter() {
  auto footer = std::make_unique<views::FlexLayoutView>();
  // ChromeLayoutProvider for providing margins.
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();

  // Set the interior margins of the footer on the left and right sides.
  footer->SetInteriorMargin(gfx::Insets::TLBR(
      0,
      chrome_layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL),
      0,
      chrome_layout_provider->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL)));

  // Set alignments for horizontal (main) and vertical (cross) axes.
  footer->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  footer->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the footer.
  footer->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);
  footer->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorWindowBackground));

  // create text button to host open in new tab
  std::unique_ptr<views::MdTextButton> label_button =
      std::make_unique<views::MdTextButton>(
          base::BindRepeating(&LensUnifiedSidePanelView::LoadResultsInNewTab,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_BUTTON_LABEL));
  label_button->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label_button->SetProminent(false);
  // set margins per UX mock
  label_button->SetProperty(views::kMarginsKey, kLensLabelButtonMargins);

  launch_button_ = footer->AddChildView(std::move(label_button));

  // Create an empty view between right and the buttons to align empty space on
  // left without hardcoding margins.
  auto container = std::make_unique<views::View>();
  container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // adding empty view to the left
  footer->AddChildView(std::move(container));

  // Install footer.
  AddChildView(std::move(footer));
}

void LensUnifiedSidePanelView::MaybeLoadURLWithParams() {
  // Ensure the side panel view has a width before loading URL. If side panel is
  // still closed (width == 0), defer loading the URL to
  // LensUnifiedSidePanelView::OnViewBoundsChanged. The nullptr check ensures we
  // don't rerender the same page on a unrelated resize event.
  if (width() == 0 || !side_panel_url_params_)
    return;

  // Manually set web contents to the size of side panel view on initial load.
  // This prevents a bug in Lens Web that renders the page as if it was 0px
  // wide.
  GetWebContents()->Resize(bounds());
  GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(*side_panel_url_params_));
  side_panel_url_params_.reset();
}

void LensUnifiedSidePanelView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // If side panel is closed when we first try to render the URL, we must wait
  // until side panel is opened. This method is called once side panel view goes
  // from 0px wide to ~320px wide. Rendering the page after side panel view
  // fully opens prevents a race condition which causes the page to load before
  // side panel is open causing the page to render as if it were 0px wide.
  MaybeLoadURLWithParams();
}

void LensUnifiedSidePanelView::SetContentVisible(bool visible) {
  web_view_->SetVisible(visible);
  loading_indicator_web_view_->SetVisible(!visible);
}

LensUnifiedSidePanelView::~LensUnifiedSidePanelView() = default;

}  // namespace lens
