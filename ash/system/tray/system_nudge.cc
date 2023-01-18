// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The corner radius of the nudge view.
constexpr int kNudgeCornerRadius = 8;

// The blur radius for the nudge view's background.
constexpr int kNudgeBlurRadius = 30;

// The margin between the edge of the screen/shelf and the nudge widget bounds.
constexpr int kNudgeMargin = 8;

constexpr base::TimeDelta kNudgeBoundsAnimationTime = base::Milliseconds(250);

// Calculates the expected bounds of nudge widget based on shelf alignment,
// hotseat state, RTL and whether to anchor to status area.
gfx::Rect CalculateWidgetBounds(const gfx::Rect& display_bounds,
                                Shelf* shelf,
                                int nudge_width,
                                int nudge_height,
                                bool anchor_status_area) {
  bool shelf_hidden = shelf->GetVisibilityState() != SHELF_VISIBLE &&
                      shelf->GetAutoHideState() == SHELF_AUTO_HIDE_HIDDEN;

  bool on_right_side;
  if (!anchor_status_area) {
    on_right_side = base::i18n::IsRTL();
  } else if (base::i18n::IsRTL()) {
    // status area is on the left side in RTL when shelf is at bottom.
    on_right_side = shelf->alignment() == ShelfAlignment::kRight;
  } else {
    on_right_side = shelf->alignment() != ShelfAlignment::kLeft;
  }

  int x;
  if (on_right_side) {
    x = display_bounds.right() - nudge_width - kNudgeMargin;
    if (shelf->alignment() == ShelfAlignment::kRight && !shelf_hidden)
      x -= ShelfConfig::Get()->shelf_size();
  } else {
    x = display_bounds.x() + kNudgeMargin;
    if (shelf->alignment() == ShelfAlignment::kLeft && !shelf_hidden)
      x += ShelfConfig::Get()->shelf_size();
  }

  int y;
  HotseatWidget* hotseat_widget = shelf->hotseat_widget();
  // Set the nudge's bounds above the hotseat when it is extended.
  if (hotseat_widget->state() == HotseatState::kExtended) {
    y = hotseat_widget->GetTargetBounds().y() - nudge_height - kNudgeMargin;
  } else {
    y = display_bounds.bottom() - nudge_height - kNudgeMargin;
    if ((shelf->alignment() == ShelfAlignment::kBottom && !shelf_hidden) ||
        shelf->alignment() == ShelfAlignment::kBottomLocked)
      y -= ShelfConfig::Get()->shelf_size();
  }

  return gfx::Rect(x, y, nudge_width, nudge_height);
}

}  // namespace

class SystemNudge::SystemNudgeView : public views::View {
 public:
  explicit SystemNudgeView(base::WeakPtr<SystemNudge> nudge) {
    DCHECK(nudge);
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        /*inside_border_insect=*/gfx::Insets(nudge->params_.nudge_padding),
        /*between_child_spacing=*/nudge->params_.icon_label_spacing);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    SetLayoutManager(std::move(layout));
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    if (features::IsBackgroundBlurEnabled())
      layer()->SetBackgroundBlur(kNudgeBlurRadius);
    layer()->SetRoundedCornerRadius({kNudgeCornerRadius, kNudgeCornerRadius,
                                     kNudgeCornerRadius, kNudgeCornerRadius});

    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetPaintToLayer();
    icon_->layer()->SetFillsBoundsOpaquely(false);
    icon_->SetSize({nudge->params_.icon_size, nudge->params_.icon_size});
    icon_->SetImage(ui::ImageModel::FromImageGenerator(
        base::BindRepeating(
            [](base::WeakPtr<SystemNudge> nudge, const ui::ColorProvider*) {
              // If `nudge` does not exist anymore, no image will be displayed.
              if (!nudge)
                return gfx::ImageSkia();

              return gfx::CreateVectorIcon(
                  nudge->GetIcon(),
                  AshColorProvider::Get()->GetContentLayerColor(
                      nudge->params_.icon_color_layer_type));
            },
            nudge),
        gfx::Size(nudge->params_.icon_size, nudge->params_.icon_size)));
    label_ = AddChildView(nudge->CreateLabelView());
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
  }

  ~SystemNudgeView() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    layer()->SetColor(ShelfConfig::Get()->GetDefaultShelfColor());
  }

  views::View* label_ = nullptr;
  views::ImageView* icon_ = nullptr;
};

SystemNudge::SystemNudge(
    const std::string& name,
    NudgeCatalogName catalog_name,
    int icon_size,
    int icon_label_spacing,
    int nudge_padding,
    bool anchor_status_area,
    AshColorProvider::ContentLayerType icon_color_layer_type)
    : root_window_(Shell::GetRootWindowForNewWindows()) {
  params_.name = name;
  params_.catalog_name = catalog_name;
  params_.icon_size = icon_size;
  params_.icon_label_spacing = icon_label_spacing;
  params_.nudge_padding = nudge_padding;
  params_.anchor_status_area = anchor_status_area;
  params_.icon_color_layer_type = icon_color_layer_type;
}

SystemNudge::~SystemNudge() = default;

void SystemNudge::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  CalculateAndSetWidgetBounds();
}

void SystemNudge::OnHotseatStateChanged(HotseatState old_state,
                                        HotseatState new_state) {
  CalculateAndSetWidgetBounds();
}

void SystemNudge::OnShelfAlignmentChanged(aura::Window* root_window,
                                          ShelfAlignment old_alignment) {
  CalculateAndSetWidgetBounds();
}

void SystemNudge::Show() {
  if (!widget_) {
    widget_ = std::make_unique<views::Widget>();

    shelf_observation_.Observe(
        RootWindowController::ForWindow(root_window_)->shelf());
    shell_observation_.Observe(Shell::Get());

    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.z_order = ui::ZOrderLevel::kFloatingWindow;
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    params.name = params_.name;
    params.layer_type = ui::LAYER_NOT_DRAWN;
    params.parent =
        root_window_->GetChildById(kShellWindowId_SettingBubbleContainer);
    widget_->Init(std::move(params));
  }

  nudge_view_ = widget_->SetContentsView(
      std::make_unique<SystemNudgeView>(/*nudge=*/weak_factory_.GetWeakPtr()));
  CalculateAndSetWidgetBounds();
  widget_->Show();

  const std::u16string accessibility_text = GetAccessibilityText();
  if (!accessibility_text.empty())
    nudge_view_->GetViewAccessibility().AnnounceText(accessibility_text);
}

void SystemNudge::Close() {
  widget_.reset();
}

void SystemNudge::CalculateAndSetWidgetBounds() {
  if (!widget_ || !root_window_ || !nudge_view_)
    return;

  DCHECK(nudge_view_->label_);

  gfx::Rect display_bounds = root_window_->bounds();
  ::wm::ConvertRectToScreen(root_window_, &display_bounds);

  gfx::Size nudge_size = nudge_view_->GetPreferredSize();
  Shelf* shelf = RootWindowController::ForWindow(root_window_)->shelf();
  gfx::Rect widget_bounds =
      CalculateWidgetBounds(display_bounds, shelf, nudge_size.width(),
                            nudge_size.height(), params_.anchor_status_area);

  // Only run the widget bounds animation if the widget's bounds have already
  // been initialized.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (widget_->GetWindowBoundsInScreen().size() != gfx::Size()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        widget_->GetLayer()->GetAnimator());
    settings->SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings->SetTransitionDuration(kNudgeBoundsAnimationTime);
    settings->SetTweenType(gfx::Tween::EASE_OUT);
  }

  widget_->SetBounds(widget_bounds);
}

}  // namespace ash