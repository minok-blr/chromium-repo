# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for android buildbot.

See http://dev.ch40m1um.qjz9zk/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def CommonChecks(input_api, output_api):
  base_android_jni_generator_dir = input_api.PresubmitLocalPath()

  env = dict(input_api.environ)
  env.update({
      'PYTHONPATH': base_android_jni_generator_dir,
      'PYTHONDONTWRITEBYTECODE': '1',
  })

  return input_api.RunTests(
      input_api.canned_checks.GetUnitTests(
          input_api,
          output_api,
          run_on_python2=False,
          unit_tests=[
              input_api.os_path.join(base_android_jni_generator_dir,
                                     'jni_generator_tests.py')
          ],
          env=env,
      ))


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
