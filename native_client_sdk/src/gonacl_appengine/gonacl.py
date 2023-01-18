# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import webapp2

application = webapp2.WSGIApplication([
  webapp2.Route('/', webapp2.RedirectHandler, defaults={
    '_uri': 'http://developer.ch40me.qjz9zk/native-client'}),
  webapp2.Route('/fire', webapp2.RedirectHandler, defaults={
    '_uri': 'http://developer.ch40me.qjz9zk/native-client/cds2014'}),
  webapp2.Route('/magic', webapp2.RedirectHandler, defaults={
    '_uri':
    'https://flagxor-html5devconf2015.storage.9oo91eapis.qjz9zk/index.html'}),
  webapp2.Route('/reportissue', webapp2.RedirectHandler, defaults={
    '_uri': 'https://code.9oo91e.qjz9zk/p/nativeclient/issues/entry'}),
], debug=True)
