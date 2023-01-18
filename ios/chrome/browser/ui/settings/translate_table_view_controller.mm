// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/translate_table_view_controller.h"

#import <Foundation/Foundation.h>

#include <memory>

#import <MaterialComponents/MaterialSnackbar.h>

#include "base/mac/foundation_util.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTranslate = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTranslate = kItemTypeEnumZero,
  ItemTypeResetTranslate,
  ItemTypeFooter,
};

const char kTranslateLearnMoreUrl[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/3214105?p=mobile_translate&ios=1";
NSString* const kTranslateSettingsCategory = @"ChromeTranslateSettings";

}  // namespace

@interface TranslateTableViewController ()<BooleanObserver> {
  // Profile preferences.
  PrefService* _prefs;  // weak
  PrefBackedBoolean* _translationEnabled;
  // The item related to the switch for the translation setting.
  TableViewSwitchItem* _translationItem;
}

@end

@implementation TranslateTableViewController

#pragma mark - Initialization

- (instancetype)initWithPrefs:(PrefService*)prefs {
  DCHECK(prefs);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _prefs = prefs;
    _translationEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_prefs
                   prefName:translate::prefs::kOfferTranslateEnabled];
    [_translationEnabled setObserver:self];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING);
  self.tableView.accessibilityIdentifier =
      @"translate_settings_view_controller";
  [self loadModel];
  self.tableView.estimatedSectionFooterHeight = kSettingsCellDefaultHeight;
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  // Translate Section
  [model addSectionWithIdentifier:SectionIdentifierTranslate];
  _translationItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeTranslate];
  _translationItem.text = l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING);
  _translationItem.on = [_translationEnabled value];
  [model addItem:_translationItem
      toSectionWithIdentifier:SectionIdentifierTranslate];

  TableViewDetailTextItem* resetTranslate =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeResetTranslate];
  resetTranslate.text = l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING_RESET);
  resetTranslate.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:resetTranslate
      toSectionWithIdentifier:SectionIdentifierTranslate];

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text = l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING_DESCRIPTION);
  footer.urls = @[ [[CrURL alloc]
      initWithGURL:google_util::AppendGoogleLocaleParam(
                       GURL(kTranslateLearnMoreUrl),
                       GetApplicationContext()->GetApplicationLocale())] ];
  [model setFooter:footer forSectionWithIdentifier:SectionIdentifierTranslate];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeTranslate: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(translateToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView =
      [super tableView:tableView viewForFooterInSection:section];
  if (SectionIdentifierTranslate ==
      [self.tableViewModel sectionIdentifierForSectionIndex:section]) {
    TableViewLinkHeaderFooterView* footer =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);
    footer.delegate = self;
  }
  return footerView;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeResetTranslate) {
    std::unique_ptr<translate::TranslatePrefs> translatePrefs(
        ChromeIOSTranslateClient::CreateTranslatePrefs(_prefs));
    translatePrefs->ResetToDefaults();
    TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
    NSString* messageText =
        l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING_RESET_NOTIFICATION);
    MDCSnackbarMessage* message =
        [MDCSnackbarMessage messageWithText:messageText];
    message.category = kTranslateSettingsCategory;
    [self.snackbarCommandsHandler showSnackbarMessage:message bottomOffset:0];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:NO];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _translationEnabled);

  // Update the item.
  _translationItem.on = [_translationEnabled value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ _translationItem ]];
}

#pragma mark - Actions

- (void)translateToggled:(id)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeTranslate
                              sectionIdentifier:SectionIdentifierTranslate];

  TableViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  TableViewSwitchCell* switchCell =
      base::mac::ObjCCastStrict<TableViewSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  DCHECK_EQ(switchCell.switchView, sender);
  BOOL isOn = switchCell.switchView.isOn;
  switchItem.on = isOn;
  [_translationEnabled setValue:isOn];
}

@end