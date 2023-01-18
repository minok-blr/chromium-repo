// Copyright (c) 2020 The ungoogled-chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROMITE_FLAG_ENTRIES_H_
#define CHROME_BROWSER_BROMITE_FLAG_ENTRIES_H_
    {"fingerprinting-client-rects-noise",
     "Enable get*ClientRects() fingerprint deception",
     "Scale the output values of Range::getClientRects() and Element::getBoundingClientRect() with a randomly selected factor in the range -0.0003% to 0.0003%, which are recomputed on every document initialization. ungoogled-chromium flag, Bromite feature.",
     kOsAll, SINGLE_VALUE_TYPE(switches::kFingerprintingClientRectsNoise)},
    {"fingerprinting-canvas-measuretext-noise",
     "Enable Canvas::measureText() fingerprint deception",
     "Scale the output values of Canvas::measureText() with a randomly selected factor in the range -0.0003% to 0.0003%, which are recomputed on every document initialization. ungoogled-chromium flag, Bromite feature.",
     kOsAll, SINGLE_VALUE_TYPE(switches::kFingerprintingCanvasMeasureTextNoise)},
    {"max-connections-per-host",
     flag_descriptions::kMaxConnectionsPerHostName,
     flag_descriptions::kMaxConnectionsPerHostDescription,
     kOsAll, MULTI_VALUE_TYPE(kMaxConnectionsPerHostChoices)},
    {"fingerprinting-canvas-image-data-noise",
     "Enable Canvas image data fingerprint deception",
     "Slightly modifies at most 10 pixels in Canvas image data extracted via JS APIs. ungoogled-chromium flag, Bromite feature.",
     kOsAll, SINGLE_VALUE_TYPE(switches::kFingerprintingCanvasImageDataNoise)},
#endif  // CHROME_BROWSER_BROMITE_FLAG_ENTRIES_H_
