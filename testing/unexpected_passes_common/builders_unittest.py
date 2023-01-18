#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import os
import sys
from typing import Any, Dict, Set, Tuple
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest

from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types
from unexpected_passes_common import multiprocessing_utils
from unexpected_passes_common import unittest_utils


class FakeFilesystemTestCaseWithFileCreation(fake_filesystem_unittest.TestCase):
  def CreateFile(self, *args, **kwargs):
    # TODO(crbug.com/1156806): Remove this and just use fs.create_file() when
    # Catapult is updated to a newer version of pyfakefs that is compatible with
    # Chromium's version.
    if hasattr(self.fs, 'create_file'):
      self.fs.create_file(*args, **kwargs)
    else:
      self.fs.CreateFile(*args, **kwargs)


class GetCiBuildersUnittest(FakeFilesystemTestCaseWithFileCreation):
  def setUp(self) -> None:
    self._builders_instance = unittest_utils.GenericBuilders(
        suite='webgl_conformance')
    self._isolate_patcher = mock.patch.object(
        self._builders_instance,
        'GetIsolateNames',
        return_value={'telemetry_gpu_integration_test'})
    self._isolate_mock = self._isolate_patcher.start()
    self.addCleanup(self._isolate_patcher.stop)

  def testJsonContentLoaded(self) -> None:
    """Tests that the correct JSON data is loaded in."""
    self.setUpPyfakefs()
    gpu_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'Android Release (Nexus 5X)': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
        'GPU Linux Builder': {},
    }
    gpu_fyi_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'ANGLE GPU Android Release (Nexus 5X)': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
        'GPU FYI Linux Builder': {},
    }
    # Should be ignored.
    tryserver_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'Trybot': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
    }
    # Also should be ignored.
    not_buildbot_json = {
        'Not buildbot': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
    }

    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'chromium.gpu.json'),
                    contents=json.dumps(gpu_json))
    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'chromium.gpu.fyi.json'),
                    contents=json.dumps(gpu_fyi_json))
    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'tryserver.gpu.json'),
                    contents=json.dumps(tryserver_json))
    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'not_buildbot.json'),
                    contents=json.dumps(not_buildbot_json))

    gpu_builders = self._builders_instance.GetCiBuilders()
    self.assertEqual(
        gpu_builders,
        set([
            data_types.BuilderEntry('Android Release (Nexus 5X)',
                                    constants.BuilderTypes.CI, False),
            data_types.BuilderEntry('ANGLE GPU Android Release (Nexus 5X)',
                                    constants.BuilderTypes.CI, False),
            data_types.BuilderEntry('GPU Linux Builder',
                                    constants.BuilderTypes.CI, False),
            data_types.BuilderEntry('GPU FYI Linux Builder',
                                    constants.BuilderTypes.CI, False),
        ]))

  def testFilterBySuite(self) -> None:
    """Tests that only builders that run the given suite are returned."""

    def SideEffect(tm: Dict[str, Any]) -> bool:
      tests = tm.get('isolated_scripts', [])
      for t in tests:
        if t.get('isolate_name') == 'foo_integration_test':
          if 'webgl_conformance' in t.get('args', []):
            return True
      return False

    self.setUpPyfakefs()
    gpu_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'Android Tester': {
            'isolated_scripts': [
                {
                    'args': [
                        'webgl_conformance',
                    ],
                    'isolate_name': 'not_telemetry',
                },
            ],
        },
        'Linux Tester': {
            'isolated_scripts': [
                {
                    'args': [
                        'not_a_suite',
                    ],
                    'isolate_name': 'foo_integration_test',
                },
            ],
        },
        'Windows Tester': {
            'isolated_scripts': [
                {
                    'args': [
                        'webgl_conformance',
                    ],
                    'isolate_name': 'foo_integration_test',
                },
            ],
        },
    }

    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'chromium.json'),
                    contents=json.dumps(gpu_json))

    with mock.patch.object(self._builders_instance,
                           '_BuilderRunsTestOfInterest',
                           side_effect=SideEffect):
      gpu_builders = self._builders_instance.GetCiBuilders()
    self.assertEqual(
        gpu_builders,
        set([
            data_types.BuilderEntry('Windows Tester', constants.BuilderTypes.CI,
                                    False)
        ]))

  def testRealContentCanBeLoaded(self) -> None:
    """Tests that *something* from the real JSON files can be loaded."""
    # This directory is not available on swarming, so if it doesn't exist, just
    # skip the test.
    if not os.path.exists(builders.TESTING_BUILDBOT_DIR):
      return
    self.assertNotEqual(len(self._builders_instance.GetCiBuilders()), 0)


class GetMirroredBuildersForCiBuilderUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._builders_instance = builders.Builders('suite', False)
    self._bb_patcher = mock.patch.object(self._builders_instance,
                                         '_GetBuildbucketOutputForCiBuilder')
    self._bb_mock = self._bb_patcher.start()
    self.addCleanup(self._bb_patcher.stop)
    self._fake_ci_patcher = mock.patch.object(self._builders_instance,
                                              'GetFakeCiBuilders',
                                              return_value={})
    self._fake_ci_mock = self._fake_ci_patcher.start()
    self.addCleanup(self._fake_ci_patcher.stop)
    self._non_chromium_patcher = mock.patch.object(
        self._builders_instance,
        'GetNonChromiumBuilders',
        return_value={'foo_non_chromium'})
    self._non_chromium_mock = self._non_chromium_patcher.start()
    self.addCleanup(self._non_chromium_patcher.stop)

  def testFakeCiBuilder(self) -> None:
    """Tests that a fake CI builder gets properly mapped."""
    self._fake_ci_mock.return_value = {
        data_types.BuilderEntry('foo_ci', constants.BuilderTypes.CI, False):
        {data_types.BuilderEntry('foo_try', constants.BuilderTypes.TRY, False)}
    }
    try_builder, found_mirror = (
        self._builders_instance._GetMirroredBuildersForCiBuilder(
            data_types.BuilderEntry('foo_ci', constants.BuilderTypes.CI,
                                    False)))
    self.assertTrue(found_mirror)
    self.assertEqual(
        try_builder,
        set([
            data_types.BuilderEntry('foo_try', constants.BuilderTypes.TRY,
                                    False)
        ]))
    self._bb_mock.assert_not_called()

  def testNoBuildbucketOutput(self) -> None:
    """Tests that a failure to get Buildbucket output is surfaced."""
    self._bb_mock.return_value = ''
    builder_entry = data_types.BuilderEntry('nonexistent',
                                            constants.BuilderTypes.CI, False)
    try_builder, found_mirror = (
        self._builders_instance._GetMirroredBuildersForCiBuilder(builder_entry))
    self.assertFalse(found_mirror)
    self.assertEqual(try_builder, set([builder_entry]))

  def testBuildbucketOutput(self):
    """Tests that Buildbucket output is parsed correctly."""
    self._bb_mock.return_value = json.dumps({
        'output': {
            'properties': {
                'mirrored_builders': [
                    'try:foo_try',
                    'try:bar_try',
                ]
            }
        }
    })
    try_builders, found_mirror = (
        self._builders_instance._GetMirroredBuildersForCiBuilder(
            data_types.BuilderEntry('foo_ci', constants.BuilderTypes.CI,
                                    False)))
    self.assertTrue(found_mirror)
    self.assertEqual(
        try_builders,
        set([
            data_types.BuilderEntry('foo_try', constants.BuilderTypes.TRY,
                                    False),
            data_types.BuilderEntry('bar_try', constants.BuilderTypes.TRY,
                                    False)
        ]))

  def testBuildbucketOutputInternal(self) -> None:
    """Tests that internal Buildbucket output is parsed correctly."""
    self._bb_mock.return_value = json.dumps({
        'output': {
            'properties': {
                'mirrored_builders': [
                    'try:foo_try',
                    'try:bar_try',
                ]
            }
        }
    })
    try_builders, found_mirror = (
        self._builders_instance._GetMirroredBuildersForCiBuilder(
            data_types.BuilderEntry('foo_ci', constants.BuilderTypes.CI, True)))
    self.assertTrue(found_mirror)
    self.assertEqual(
        try_builders,
        set([
            data_types.BuilderEntry('foo_try', constants.BuilderTypes.TRY,
                                    True),
            data_types.BuilderEntry('bar_try', constants.BuilderTypes.TRY, True)
        ]))


class GetTryBuildersUnittest(FakeFilesystemTestCaseWithFileCreation):
  def setUp(self) -> None:
    self._builders_instance = builders.Builders('suite', False)
    self._get_patcher = mock.patch.object(self._builders_instance,
                                          '_GetMirroredBuildersForCiBuilder')
    self._get_mock = self._get_patcher.start()
    self.addCleanup(self._get_patcher.stop)
    self._runs_test_patcher = mock.patch.object(self._builders_instance,
                                                '_BuilderRunsTestOfInterest')
    self._runs_test_mock = self._runs_test_patcher.start()
    self.addCleanup(self._runs_test_patcher.stop)
    self._pool_patcher = mock.patch.object(multiprocessing_utils,
                                           'GetProcessPool')
    self._pool_mock = self._pool_patcher.start()
    self._pool_mock.return_value = unittest_utils.FakePool()
    self.addCleanup(self._pool_patcher.stop)

    self.setUpPyfakefs()
    # Make sure the directory exists.
    self.CreateFile(
        os.path.join(builders.TESTING_BUILDBOT_DIR, 'placeholder.txt'))

  def testMirrorNoOutputCausesFailure(self) -> None:
    """Tests that a failure to get Buildbot output raises an exception."""
    builder = data_types.BuilderEntry('foo_ci', constants.BuilderTypes.CI,
                                      False)
    self._get_mock.return_value = (set([builder]), False)
    self._runs_test_mock.return_value = True
    with self.assertRaises(RuntimeError):
      self._builders_instance.GetTryBuilders([builder])

  def testMirrorOutputReturned(self) -> None:
    """Tests that parsed, mirrored builders get returned on success."""

    def SideEffect(ci_builder: data_types.BuilderEntry
                   ) -> Tuple[Set[data_types.BuilderEntry], bool]:
      b = [
          data_types.BuilderEntry(ci_builder.name.replace('ci', 'try'),
                                  constants.BuilderTypes.TRY, False),
          data_types.BuilderEntry(ci_builder.name.replace('ci', 'try2'),
                                  constants.BuilderTypes.TRY, False),
      ]
      return set(b), True

    self._get_mock.side_effect = SideEffect
    self._runs_test_mock.return_value = False
    mirrored_builders = self._builders_instance.GetTryBuilders([
        data_types.BuilderEntry('foo_ci', constants.BuilderTypes.CI, False),
        data_types.BuilderEntry('bar_ci', constants.BuilderTypes.CI, False),
    ])
    self.assertEqual(
        mirrored_builders,
        set([
            data_types.BuilderEntry('foo_try', constants.BuilderTypes.TRY,
                                    False),
            data_types.BuilderEntry('foo_try2', constants.BuilderTypes.TRY,
                                    False),
            data_types.BuilderEntry('bar_try', constants.BuilderTypes.TRY,
                                    False),
            data_types.BuilderEntry('bar_try2', constants.BuilderTypes.TRY,
                                    False),
        ]))

  def testDedicatedJsonContentLoaded(self) -> None:
    """Tests that tryserver JSON content is loaded."""

    def SideEffect(test_spec: Dict[str, Any]) -> bool:
      # Treat non-empty test specs as valid.
      return bool(test_spec)

    self._runs_test_mock.side_effect = SideEffect
    # Should be ignored.
    gpu_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'Android Release (Nexus 5X)': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
        'GPU Linux Builder': {},
    }
    # Should be ignored.
    gpu_fyi_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'ANGLE GPU Android Release (Nexus 5X)': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
        'GPU FYI Linux Builder': {},
    }
    tryserver_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'Trybot': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
        'Trybot Empty': {},
    }
    # Also should be ignored.
    not_buildbot_json = {
        'Not buildbot': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
    }

    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'chromium.gpu.json'),
                    contents=json.dumps(gpu_json))
    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'chromium.gpu.fyi.json'),
                    contents=json.dumps(gpu_fyi_json))
    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'tryserver.gpu.json'),
                    contents=json.dumps(tryserver_json))
    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'not_buildbot.json'),
                    contents=json.dumps(not_buildbot_json))

    gpu_builders = self._builders_instance.GetTryBuilders({})
    self.assertEqual(
        gpu_builders,
        set([
            data_types.BuilderEntry('Trybot', constants.BuilderTypes.TRY,
                                    False),
        ]))

  def testDedicatedFilterBySuite(self) -> None:
    """Tests that only builders that run the given suite are returned."""

    def SideEffect(tm: Dict[str, Any]) -> bool:
      tests = tm.get('isolated_scripts', [])
      for t in tests:
        if t.get('isolate_name') == 'foo_integration_test':
          if 'webgl_conformance' in t.get('args', []):
            return True
      return False

    self._runs_test_mock.side_effect = SideEffect
    gpu_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'Android Tester': {
            'isolated_scripts': [
                {
                    'args': [
                        'webgl_conformance',
                    ],
                    'isolate_name': 'not_telemetry',
                },
            ],
        },
        'Linux Tester': {
            'isolated_scripts': [
                {
                    'args': [
                        'not_a_suite',
                    ],
                    'isolate_name': 'foo_integration_test',
                },
            ],
        },
        'Windows Tester': {
            'isolated_scripts': [
                {
                    'args': [
                        'webgl_conformance',
                    ],
                    'isolate_name': 'foo_integration_test',
                },
            ],
        },
    }

    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'tryserver.chromium.json'),
                    contents=json.dumps(gpu_json))

    gpu_builders = self._builders_instance.GetTryBuilders({})
    self.assertEqual(
        gpu_builders,
        set([
            data_types.BuilderEntry('Windows Tester',
                                    constants.BuilderTypes.TRY, False)
        ]))

  def testDedicatedAndMirroredCombined(self) -> None:
    """Tests that both dedicated and mirrored trybots are returned."""

    def SideEffect(_: Any) -> Tuple[Set[data_types.BuilderEntry], bool]:
      return set({
          data_types.BuilderEntry('mirrored_trybot', constants.BuilderTypes.TRY,
                                  False)
      }), True

    self._get_mock.side_effect = SideEffect
    self._runs_test_mock.return_value = True
    tryserver_json = {
        'AAAAA1 AUTOGENERATED FILE DO NOT EDIT': {},
        'Trybot': {
            'isolated_scripts': [{
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name':
                'telemetry_gpu_integration_test',
            }],
        },
    }

    self.CreateFile(os.path.join(builders.TESTING_BUILDBOT_DIR,
                                 'tryserver.chromium.json'),
                    contents=json.dumps(tryserver_json))

    try_builders = self._builders_instance.GetTryBuilders({
        data_types.BuilderEntry('ci_builder', constants.BuilderTypes.CI, False)
    })
    self.assertEqual(
        try_builders, {
            data_types.BuilderEntry('mirrored_trybot',
                                    constants.BuilderTypes.TRY, False),
            data_types.BuilderEntry('Trybot', constants.BuilderTypes.TRY, False)
        })


if __name__ == '__main__':
  unittest.main(verbosity=2)