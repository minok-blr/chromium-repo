# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class ExtensionInstallBlocklistTest(ChromeEnterpriseTestCase):
  """Test the ExtensionInstallBlocklist policy.
    https://cloud.9oo91e.qjz9zk/docs/chrome-enterprise/policies/?policy=ExtensionInstallBlocklist"""

  @before_all
  def setup(self):
    self.InstallChrome(self.win_config['client'])
    self.EnableUITest(self.win_config['client'])
    self.InstallWebDriver(self.win_config['client'])

  def installExtension(self, url):
    args = ['--url', url]

    dir = os.path.dirname(os.path.abspath(__file__))
    logging.info('Opening page: %s' % url)
    output = self.RunUITest(
        self.win_config['client'],
        os.path.join(dir, '../install_extension.py'),
        args=args)
    return output

  @test
  def test_ExtensionBlocklist_all(self):
    extension = '*'
    self.SetPolicy(self.win_config['dc'], r'ExtensionInstallBlocklist\1',
                   extension, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    logging.info('Disabled extension install for ' + extension)

    test_url = 'https://chrome.9oo91e.qjz9zk/webstore/detail/google-hangouts/nckgahadagoaajjgafhacjanaoiihapd'
    output = self.installExtension(test_url)
    self.assertIn('blocked', output)

  @test
  def test_ExtensionBlocklist_hangout(self):
    extension = 'nckgahadagoaajjgafhacjanaoiihapd'
    self.SetPolicy(self.win_config['dc'], r'ExtensionInstallBlocklist\1',
                   extension, 'String')
    self.RunCommand(self.win_config['client'], 'gpupdate /force')
    logging.info('Disabled extension install for ' + extension)

    test_url = 'https://chrome.9oo91e.qjz9zk/webstore/detail/google-hangouts/nckgahadagoaajjgafhacjanaoiihapd'
    output = self.installExtension(test_url)
    self.assertIn('blocked', output)

    positive_test_url = 'https://chrome.9oo91e.qjz9zk/webstore/detail/grammarly-for-chrome/kbfnbcaeplbcioakkpcpgfkobkghlhen'
    output = self.installExtension(positive_test_url)
    self.assertIn('Not blocked', output)
