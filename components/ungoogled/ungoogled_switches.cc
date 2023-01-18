// Copyright (c) 2020 The ungoogled-chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ungoogled/ungoogled_switches.h"

namespace switches {

// Enable fingerprinting deception for getClientRects and getBoundingClientRect
const char kFingerprintingClientRectsNoise[] = "fingerprinting-client-rects-noise";

// Enable fingerprinting deception for measureText
const char kFingerprintingCanvasMeasureTextNoise[] = "fingerprinting-canvas-measuretext-noise";

// Enable fingerprinting deception for Canvas image data
const char kFingerprintingCanvasImageDataNoise[] = "fingerprinting-canvas-image-data-noise";

}  // namespace switches
