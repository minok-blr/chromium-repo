// Copyright (c) 2020 The ungoogled-chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNGOOGLED_FLAG_ENTRIES_H_
#define CHROME_BROWSER_UNGOOGLED_FLAG_ENTRIES_H_
    {"set-ipv6-probe-false",
     "SetIpv6ProbeFalse",
     "Forces the result of the browser's IPv6 probing (i.e. IPv6 connectivity test) to be unsuccessful. This causes IPv4 addresses to be prioritized over IPv6 addresses. Without this flag, the probing result is set to be successful, which causes IPv6 to be used over IPv4 when possible. ungoogled-chromium flag.",
     kOsAll, FEATURE_VALUE_TYPE(net::features::kSetIpv6ProbeFalse)},
    {"extension-mime-request-handling",
     "Handling of extension MIME type requests",
     "Used when deciding how to handle a request for a CRX or User Script MIME type. ungoogled-chromium flag.",
     kOsAll, MULTI_VALUE_TYPE(kExtensionHandlingChoices)},
    {"disable-search-engine-collection",
     "Disable search engine collection",
     "Prevents search engines from being added automatically. ungoogled-chromium flag.",
     kOsAll, SINGLE_VALUE_TYPE("disable-search-engine-collection")},
    {"disable-beforeunload",
     "Disable beforeunload",
     "Disables JavaScript dialog boxes triggered by beforeunload. ungoogled-chromium flag.",
     kOsAll, SINGLE_VALUE_TYPE("disable-beforeunload")},
    {"force-punycode-hostnames",
     "Force punycode hostnames",
     "Force punycode in hostnames instead of Unicode when displaying Internationalized Domain Names (IDNs). ungoogled-chromium flag.",
     kOsAll, SINGLE_VALUE_TYPE("force-punycode-hostnames")},
    {"show-avatar-button",
     "Show avatar/people/profile button",
     "Show avatar/people/profile button in the browser toolbar. ungoogled-chromium flag.",
     kOsDesktop, MULTI_VALUE_TYPE(kShowAvatarButtonChoices)},
    {"hide-crashed-bubble",
     "Hide crashed bubble",
     "Hides the bubble box with the message \"Restore Pages? Chromium didn't shut down correctly.\" that shows on startup after the browser did not exit cleanly. ungoogled-chromium flag.",
     kOsAll, SINGLE_VALUE_TYPE("hide-crashed-bubble")},
    {"scroll-tabs",
     "Scroll switches tab",
     "Switch to the left/right tab if the wheel-scroll happens over the tabstrip, or the empty space beside the tabstrip. ungoogled-chromium flag.",
     kOsDesktop, MULTI_VALUE_TYPE(kScrollEventChangesTab)},
    {"bookmark-bar-ntp",
     "Bookmark Bar on New-Tab-Page",
     "Disable the Bookmark Bar on the New-Tab-Page. ungoogled-chromium flag.",
     kOsDesktop, MULTI_VALUE_TYPE(kBookmarkBarNewTab)},
    {"omnibox-autocomplete-filtering",
     "Omnibox Autocomplete Filtering",
     "Restrict omnibox autocomplete results to a combination of search suggestions (if enabled), bookmarks, and internal chrome pages. ungoogled-chromium flag.",
     kOsAll, MULTI_VALUE_TYPE(kOmniboxAutocompleteFiltering)},
    {"close-window-with-last-tab",
     "Close window with last tab",
     "Determines whether a window should close once the last tab is closed. ungoogled-chromium flag.",
     kOsDesktop, MULTI_VALUE_TYPE(kCloseWindowWithLastTab)},
    {"popups-to-tabs",
     "Popups to tabs",
     "Makes popups open in new tabs. ungoogled-chromium flag",
     kOsAll, SINGLE_VALUE_TYPE("popups-to-tabs")},
    {"keep-old-history",
     "Keep old history",
     "Keep history older than 3 months. ungoogled-chromium flag",
     kOsAll, SINGLE_VALUE_TYPE("keep-old-history")},
    {"clear-data-on-exit",
     "Clear data on exit",
     "Clears all browsing data on exit. ungoogled-chromium flag",
     kOsDesktop, FEATURE_VALUE_TYPE(browsing_data::features::kClearDataOnExit)},
    {"remove-tabsearch-button",
     "Remove Tabsearch Button",
     "Removes the tabsearch button from the tabstrip. ungoogled-chromium flag",
     kOsDesktop, SINGLE_VALUE_TYPE("remove-tabsearch-button")},
    {"disable-qr-generator",
     "Disable QR Generator",
     "Disables the QR generator for sharing page links. ungoogled-chromium flag",
     kOsDesktop, FEATURE_VALUE_TYPE(kDisableQRGenerator)},
    {"remove-grab-handle",
     "Remove Grab Handle",
     "Removes the reserved empty space in the tabstrip for moving the window. ungoogled-chromium flag",
     kOsDesktop, SINGLE_VALUE_TYPE("remove-grab-handle")},
    {"close-confirmation",
     "Close Confirmation",
     "Show a warning prompt when closing the browser window. ungoogled-chromium flag",
     kOsDesktop, MULTI_VALUE_TYPE(kCloseConfirmation)},
    {"custom-ntp",
     "Custom New Tab Page",
     "Allows setting a custom URL for the new tab page. Value can be internal (e.g. `about:blank`), external (e.g. `example.com`), or local (e.g. `file:///tmp/startpage.html`). This applies for incognito windows as well when not set to a `chrome://` internal page. ungoogled-chromium flag",
     kOsDesktop, ORIGIN_LIST_VALUE_TYPE("custom-ntp", "")},
    {"tab-hover-cards",
     "Tab Hover Cards",
     "Allows removing the tab hover cards or using a tooltip as a replacement. ungoogled-chromium flag.",
     kOsDesktop, MULTI_VALUE_TYPE(kTabHoverCards)},
    {"hide-tab-close-buttons",
     "Hide tab close buttons",
     "Hides the close buttons on tabs. ungoogled-chromium flag.",
     kOsDesktop, SINGLE_VALUE_TYPE("hide-tab-close-buttons")},
    {"referrer-directive",
     "Referrer directive",
     "Allows setting a custom directive for referrer headers. The no cross-origin referrer option removes all cross-origin referrers, the minimal option removes all cross-origin referrers and strips same-origin referrers down to the origin, and the no referrers option removes all referrers. ungoogled-chromium flag.",
     kOsAll, MULTI_VALUE_TYPE(kReferrerDirective)},
    {"disable-grease-tls",
     "Disable GREASE for TLS",
     "Turn off GREASE (Generate Random Extensions And Sustain Extensibility) for TLS connections. ungoogled-chromium flag.",
     kOsAll, SINGLE_VALUE_TYPE("disable-grease-tls")},
    {"http-accept-header",
     "Custom HTTP Accept Header",
     "Set a custom value for the Accept header which is sent by the browser with every HTTP request.  (e.g. `text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8`). ungoogled-chromium flag.",
     kOsAll, ORIGIN_LIST_VALUE_TYPE("http-accept-header", "")},
    {"disable-sharing-hub",
     "Disable Sharing Hub",
     "Disables the sharing hub button. ungoogled-chromium flag.",
     kOsDesktop, SINGLE_VALUE_TYPE("disable-sharing-hub")},
    {"hide-sidepanel-button",
     "Hide SidePanel Button",
     "Hides the SidePanel Button. ungoogled-chromium flag.",
     kOsDesktop, SINGLE_VALUE_TYPE("hide-sidepanel-button")},
    {"disable-link-drag",
     "Disable link drag",
     "Prevents dragging of links and selected text. ungoogled-chromium flag.",
     kOsDesktop, FEATURE_VALUE_TYPE(blink::features::kDisableLinkDrag)},
    {"hide-extensions-menu",
     "Hide Extensions Menu",
     "Hides the extensions menu (the puzzle piece icon). ungoogled-chromium flag.",
     kOsDesktop, SINGLE_VALUE_TYPE("hide-extensions-menu")},
#endif  // CHROME_BROWSER_UNGOOGLED_FLAG_ENTRIES_H_
