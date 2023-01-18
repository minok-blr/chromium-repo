// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/api/automation.h"
#include "extensions/renderer/api/automation/automation_ax_tree_wrapper.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "ipc/ipc_message.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_tree.h"
#include "v8/include/v8.h"

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace ui {
struct AXEvent;
}

namespace extensions {

class AutomationInternalCustomBindings;
class AutomationMessageFilter;
class NativeExtensionBindingsSystem;

struct TreeChangeObserver {
  int id;
  api::automation::TreeChangeObserverFilter filter;
};

// The native component of custom bindings for the chrome.automationInternal
// API.
class AutomationInternalCustomBindings : public ObjectBackedNativeHandler {
 public:
  AutomationInternalCustomBindings(
      ScriptContext* context,
      NativeExtensionBindingsSystem* bindings_system);

  AutomationInternalCustomBindings(const AutomationInternalCustomBindings&) =
      delete;
  AutomationInternalCustomBindings& operator=(
      const AutomationInternalCustomBindings&) = delete;

  ~AutomationInternalCustomBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  void OnMessageReceived(const IPC::Message& message);

  AutomationAXTreeWrapper* GetAutomationAXTreeWrapperFromTreeID(
      ui::AXTreeID tree_id) const;

  // Given a tree (|in_out_tree_wrapper|) and a node, returns the parent.
  // If |node| is the root of its tree, the return value will be the host
  // node of the parent tree and |in_out_tree_wrapper| will be updated to
  // point to that parent tree.
  //
  // Optionally, |should_use_app_id|, if true, considers
  // ax::mojom::IntAttribute::kChildTreeNodeAppId when moving to ancestors.
  // |requires_unignored|, if true, keeps moving to ancestors until an unignored
  // ancestor parent is found.
  ui::AXNode* GetParent(ui::AXNode* node,
                        AutomationAXTreeWrapper** in_out_tree_wrapper,
                        bool should_use_app_id = true,
                        bool requires_unignored = true) const;

  // Gets the hosting node in a parent tree.
  ui::AXNode* GetHostInParentTree(
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;

  // Gets the root(s) of a node's child tree. Multiple roots can occur when the
  // child tree uses ax::mojom::StringAttribute::kAppId.
  std::vector<ui::AXNode*> GetRootsOfChildTree(ui::AXNode* node) const;

  ui::AXNode* GetNextInTreeOrder(
      ui::AXNode* start,
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;
  ui::AXNode* GetPreviousInTreeOrder(
      ui::AXNode* start,
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;

  ScriptContext* context() const {
    return ObjectBackedNativeHandler::context();
  }

  void SendNodesRemovedEvent(ui::AXTree* tree, const std::vector<int>& ids);
  bool SendTreeChangeEvent(api::automation::TreeChangeType change_type,
                           ui::AXTree* tree,
                           ui::AXNode* node);
  void SendAutomationEvent(
      ui::AXTreeID tree_id,
      const gfx::Point& mouse_location,
      const ui::AXEvent& event,
      absl::optional<ui::AXEventGenerator::Event> generated_event_type =
          absl::optional<ui::AXEventGenerator::Event>());

  void MaybeSendFocusAndBlur(
      AutomationAXTreeWrapper* tree,
      const ExtensionMsg_AccessibilityEventBundleParams& event_bundle);

  absl::optional<gfx::Rect> GetAccessibilityFocusedLocation() const;

  void SendAccessibilityFocusedLocationChange(const gfx::Point& mouse_location);

 private:
  friend class AutomationInternalCustomBindingsTest;

  // ObjectBackedNativeHandler overrides:
  void Invalidate() override;

  // Returns whether this extension has the "interact" permission set (either
  // explicitly or implicitly after manifest parsing).
  void IsInteractPermitted(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns an object with bindings that will be added to the
  // chrome.automation namespace.
  void GetSchemaAdditions(const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that needs access to accessibility trees. This
  // enables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process.
  void StartCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that turns off accessibility trees. This
  // disables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process and clears all existing tree state.
  void StopCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Called when an accessibility tree is destroyed and needs to be
  // removed from our cache.
  // Args: string ax_tree_id
  void DestroyAccessibilityTree(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void AddTreeChangeObserver(const v8::FunctionCallbackInfo<v8::Value>& args);

  void RemoveTreeChangeObserver(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void GetFocus(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Given an initial AutomationAXTreeWrapper, return the
  // AutomationAXTreeWrapper and node of the focused node within this tree or a
  // focused descendant tree.
  bool GetFocusInternal(AutomationAXTreeWrapper* top_tree,
                        AutomationAXTreeWrapper** out_tree,
                        ui::AXNode** out_node);

  void RouteTreeIDFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       AutomationAXTreeWrapper* tree_wrapper));

  void RouteNodeIDFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node)> callback);
  void RouteNodeIDPlusAttributeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AXTree* tree,
                       ui::AXNode* node,
                       const std::string& attribute_name));
  void RouteNodeIDPlusRangeFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         int start,
                         int end,
                         bool clipped)> callback);
  void RouteNodeIDPlusStringBoolFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         const std::string& strVal,
                         bool boolVal)> callback);
  void RouteNodeIDPlusDimensionsFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         int start,
                         int end,
                         int width,
                         int height)> callback);
  void RouteNodeIDPlusEventFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         api::automation::EventType event_type)> callback);

  //
  // Access the cached accessibility trees and properties of their nodes.
  //

  // Args: string ax_tree_id, int node_id, Returns: int child_id.
  void GetChildIDAtIndex(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns: string tree_id and int node_id of a node which has global
  // accessibility focus.
  void GetAccessibilityFocus(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id.
  void SetDesktopID(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id, int node_id
  // Returns: JS object with a map from html attribute key to value.
  void GetHtmlAttributes(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id, int node_id
  // Returns: JS object with a string key for each state flag that's set.
  void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Creates the backing AutomationPosition native object given a request from
  // javascript.
  // Args: string ax_tree_id, int node_id, int offset, bool is_downstream
  // Returns: JS object with bindings back to the native AutomationPosition.
  void CreateAutomationPosition(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  //
  // Helper functions.
  //

  // Handle accessibility events from the browser process.
  void OnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events,
      bool is_active_profile);
  void OnAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params);

  void UpdateOverallTreeChangeObserverFilter();

  void SendChildTreeIDEvent(ui::AXTreeID child_tree_id);

  std::string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const;

  std::vector<int> CalculateSentenceBoundary(
      AutomationAXTreeWrapper* tree_wrapper,
      ui::AXNode* node,
      bool start_boundary);

  // Adjust the bounding box of a node from local to global coordinates,
  // walking up the parent hierarchy to offset by frame offsets and
  // scroll offsets.
  // If |clip_bounds| is false, the bounds of the node will not be clipped
  // to the ancestors bounding boxes if needed. Regardless of clipping, results
  // are returned in global coordinates.
  gfx::Rect ComputeGlobalNodeBounds(AutomationAXTreeWrapper* tree_wrapper,
                                    ui::AXNode* node,
                                    gfx::RectF local_bounds = gfx::RectF(),
                                    bool* offscreen = nullptr,
                                    bool clip_bounds = true) const;

  void TreeEventListenersChanged(AutomationAXTreeWrapper* tree_wrapper);

  void MaybeSendOnAllAutomationEventListenersRemoved();

  std::map<ui::AXTreeID, std::unique_ptr<AutomationAXTreeWrapper>>
      tree_id_to_tree_wrapper_map_;
  scoped_refptr<AutomationMessageFilter> message_filter_;
  bool is_active_profile_;
  std::vector<TreeChangeObserver> tree_change_observers_;
  // A bit-map of api::automation::TreeChangeObserverFilter.
  int tree_change_observer_overall_filter_;
  NativeExtensionBindingsSystem* bindings_system_;
  bool should_ignore_context_;

  // The global focused tree id.
  ui::AXTreeID focus_tree_id_;

  // The global focused node id.
  int32_t focus_id_ = -1;

  // The global accessibility focused id set by a js client. Differs from focus
  // as used in ui::AXTree.
  ui::AXTreeID accessibility_focused_tree_id_ = ui::AXTreeIDUnknown();

  // Keeps track  of the single desktop tree, if it exists.
  ui::AXTreeID desktop_tree_id_ = ui::AXTreeIDUnknown();

  // Keeps track of all trees with event listeners.
  std::set<ui::AXTreeID> trees_with_event_listeners_;

  base::RepeatingCallback<void(api::automation::EventType)>
      notify_event_for_testing_;

  base::WeakPtrFactory<AutomationInternalCustomBindings> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_