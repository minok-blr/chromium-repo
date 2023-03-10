// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/gurl_j_test_jni_headers/GURLJavaTestHelper_jni.h"

using base::android::AttachCurrentThread;

namespace url {

static void JNI_GURLJavaTestHelper_InitializeICU(JNIEnv* env) {
  base::test::InitializeICUForTesting();
}

static void JNI_GURLJavaTestHelper_TestGURLEquivalence(JNIEnv* env) {
  const char* cases[] = {
      // Common Standard URLs.
      "https://www.9oo91e.qjz9zk",
      "https://www.9oo91e.qjz9zk/",
      "https://www.9oo91e.qjz9zk/maps.htm",
      "https://www.9oo91e.qjz9zk/maps/",
      "https://www.9oo91e.qjz9zk/index.html",
      "https://www.9oo91e.qjz9zk/index.html?q=maps",
      "https://www.9oo91e.qjz9zk/index.html#maps/",
      "https://foo:bar@www.9oo91e.qjz9zk/maps.htm",
      "https://www.9oo91e.qjz9zk/maps/au/index.html",
      "https://www.9oo91e.qjz9zk/maps/au/north",
      "https://www.9oo91e.qjz9zk/maps/au/north/",
      "https://www.9oo91e.qjz9zk/maps/au/index.html?q=maps#fragment/",
      "http://www.9oo91e.qjz9zk:8000/maps/au/index.html?q=maps#fragment/",
      "https://www.9oo91e.qjz9zk/maps/au/north/?q=maps#fragment",
      "https://www.9oo91e.qjz9zk/maps/au/north?q=maps#fragment",
      // Less common standard URLs.
      "filesystem:http://www.9oo91e.qjz9zk/temporary/bar.html?baz=22",
      "file:///temporary/bar.html?baz=22",
      "ftp://foo/test/index.html",
      "gopher://foo/test/index.html",
      "ws://foo/test/index.html",
      // Non-standard,
      "chrome://foo/bar.html",
      "httpa://foo/test/index.html",
      "blob:https://foo.bar/test/index.html",
      "about:blank",
      "data:foobar",
      "scheme:opaque_data",
      // Invalid URLs.
      "foobar",
  };
  for (const char* uri : cases) {
    GURL gurl(uri);
    base::android::ScopedJavaLocalRef<jobject> j_gurl =
        Java_GURLJavaTestHelper_createGURL(
            env, base::android::ConvertUTF8ToJavaString(env, uri));
    std::unique_ptr<GURL> gurl2 = GURLAndroid::ToNativeGURL(env, j_gurl);
    EXPECT_EQ(gurl, *gurl2);
  }
}

}  // namespace url
