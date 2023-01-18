// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_MOJO_TRAP_H_
#define MOJO_CORE_IPCZ_DRIVER_MOJO_TRAP_H_

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

// Mojo traps are more complex than ipcz traps. A Mojo trap is approximately
// equivalent to a *collection* of ipcz traps (which Mojo would call "triggers"
// within a trap) sharing a common event handler.
//
// A Mojo trap can only be armed while all of its triggers' conditions are
// simultaneously unsatisfied. This object emulates that behavior well enough to
// suit Chromium's needs.
class MojoTrap : public Object<MojoTrap> {
 public:
  explicit MojoTrap(MojoTrapEventHandler handler);

  static Type object_type() { return kMojoTrap; }

  // Registers a new trigger on this trap. Each trigger corresponds to an active
  // ipcz trap when this Mojo trap is armed.
  MojoResult AddTrigger(MojoHandle handle,
                        MojoHandleSignals signals,
                        MojoTriggerCondition condition,
                        uintptr_t trigger_context);

  // Unregisters a trigger from the trap. If the trigger still has an ipcz trap
  // installed on `handle`, any event it may eventually fire will be ignored.
  MojoResult RemoveTrigger(uintptr_t trigger_context);

  // Attempts to arm this Mojo trap. Successful arming means that for every
  // trigger added, we can install a corresponding ipcz trap.
  MojoResult Arm(MojoTrapEvent* blocking_events, uint32_t* num_blocking_events);

  // ObjectBase:
  void Close() override;

 private:
  struct Trigger;

  ~MojoTrap() override;

  static void TrapEventHandler(const IpczTrapEvent* event);
  static void TrapRemovalEventHandler(const IpczTrapEvent* event);

  void HandleEvent(const IpczTrapEvent& event);
  void HandleTrapRemoved(const IpczTrapEvent& event);

  // Attempts to arm a single trigger by creating an ipcz trap for it.
  IpczResult ArmTrigger(Trigger& trigger,
                        IpczTrapConditionFlags* satisfied_flags,
                        IpczPortalStatus* status);

  void NotifyTriggerRemoved(Trigger&);

  const MojoTrapEventHandler handler_;

  base::Lock lock_;

  using TriggerMap = base::flat_map<uintptr_t, scoped_refptr<Trigger>>;
  TriggerMap triggers_ GUARDED_BY(lock_);

  // Trigger prioritization proceeds in a round-robin fashion across consecutive
  // Arm() invocations. This iterator caches the most recently prioritized
  // entry.
  //
  // SUBTLE: Because it is invalidated by mutations to `triggers_`, this MUST
  // be reset any time a trigger is inserted or removed.
  TriggerMap::iterator next_trigger_ GUARDED_BY(lock_) = triggers_.end();

  bool armed_ GUARDED_BY(lock_) = false;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_MOJO_TRAP_H_