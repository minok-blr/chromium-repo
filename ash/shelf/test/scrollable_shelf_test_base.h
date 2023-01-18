// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_TEST_SCROLLABLE_SHELF_TEST_BASE_H_
#define ASH_SHELF_TEST_SCROLLABLE_SHELF_TEST_BASE_H_

#include "ash/public/cpp/shelf_types.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_color_generator.h"

namespace ash {
class ScrollableShelfView;
class ShelfView;
class ShelfViewTestAPI;

// Shared by the scrollable shelf pixel and non-pixel tests.
class ScrollableShelfTestBase : public AshTestBase {
 public:
  ScrollableShelfTestBase();
  ScrollableShelfTestBase(const ScrollableShelfTestBase&) = delete;
  ScrollableShelfTestBase& operator=(const ScrollableShelfTestBase&) = delete;
  ~ScrollableShelfTestBase() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Pins some app icons to shelf. If `use_alternative_color` is true, the
  // neighboring shelf app icons are of different colors.
  void PopulateAppShortcut(int number, bool use_alternative_color = false);

  // Keeps pinning app icons to shelf until the shelf arrow button shows.
  // If `use_alternative_color` is true, the neighboring shelf app icons are of
  // different colors.
  void AddAppShortcutsUntilOverflow(bool use_alternative_color = false);

  // Adds a shelf item of the specified type and color.
  ShelfID AddAppShortcutWithIconColor(ShelfItemType item_type, SkColor color);

  ScrollableShelfView* scrollable_shelf_view_ = nullptr;
  ShelfView* shelf_view_ = nullptr;
  std::unique_ptr<ShelfViewTestAPI> test_api_;

  // Used as the id of the next pinned app. Updates when an app is pinned.
  int id_ = 0;

  AshTestColorGenerator icon_color_generator_{/*default_color=*/SK_ColorRED};
};

}  // namespace ash

#endif  // ASH_SHELF_TEST_SCROLLABLE_SHELF_TEST_BASE_H_