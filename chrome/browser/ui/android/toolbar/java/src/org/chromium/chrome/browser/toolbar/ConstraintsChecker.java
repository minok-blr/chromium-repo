// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Watches a constraints supplier for the next time the browser controls are unlocked,
 * and then tells the {@link ViewResourceAdapter} to generate a resource.
 */
public class ConstraintsChecker implements Callback<Integer> {
    @NonNull
    private final ViewResourceAdapter mViewResourceAdapter;
    @NonNull
    private final ObservableSupplier<Integer> mConstraintsSupplier;

    /**
     * @param viewResourceAdapter The target to notify when a capture is needed.
     * @param constraintsSupplier The underlying supplier for the state of constraints.
     */
    public ConstraintsChecker(@NonNull ViewResourceAdapter viewResourceAdapter,
            @NonNull ObservableSupplier<Integer> constraintsSupplier) {
        mViewResourceAdapter = viewResourceAdapter;
        mConstraintsSupplier = constraintsSupplier;
    }

    /**
     * Returns whether the controls are currently locked. When the supplier returns null and we are
     * uncertain if the controls are locked or not, true is also returned here to be safe.
     */
    public boolean areControlsLocked() {
        Integer constraints = mConstraintsSupplier.get();
        return constraints == null
                || constraints.intValue() == org.chromium.cc.input.BrowserControlsState.SHOWN;
    }

    /**
     * Sets up an observer that will request a resource from the {@link ViewResourceAdapter} when
     * the controls become unlocked. Depending on what is going on with the browser, this could take
     * a long time to result in a call.
     */
    public void scheduleRequestResourceOnUnlock() {
        mConstraintsSupplier.addObserver(this);
    }

    @Override
    public void onResult(Integer result) {
        if (!areControlsLocked()) {
            mConstraintsSupplier.removeObserver(this);
            mViewResourceAdapter.onResourceRequested();
        }
    }
}