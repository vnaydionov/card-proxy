# -*- coding: utf-8 -*-

import time
import urllib2
import urllib
import re
import random
import os
import xml.etree.ElementTree as et
import logging

log = logging.getLogger('secvault_api')


def response_wrapper(f):
    def timer(*args):
        s_time = time.time()
        response = f(*args)
        f_time = time.time()
        response_xml = et.XML(response)
        status = response_xml.findtext('status')
        return status, response_xml, f_time - s_time
    return timer


class SecureVaultApi(object):
    def __init__(self, server_uri, user, passwd, domain, timeout=2.0, method='POST'):
        self.server_uri = server_uri
        self.user = user
        self.passwd = passwd
        self.domain = domain
        self.timeout = timeout
        self.method = method

    def try_call(self, uri, user_data=None):
        data = {k: v for k, v in (user_data or {}).iteritems()}
        if 'user' not in data:
            data['user'] = self.user
        if 'passwd' not in data:
            data['passwd'] = self.passwd
        if 'domain' not in data:
            data['domain'] = self.domain
        log.debug('Connect to [%s]...', uri)
        log.debug('Data: %r', data)
        data_encoded = urllib.urlencode(data)
        if self.method == 'POST':
            resp = urllib2.urlopen(uri, data_encoded, timeout=self.timeout)
        elif self.method == 'GET':
            if len(data):
                uri += '?' + data_encoded
            resp = urllib2.urlopen(uri, timeout=self.timeout)
        else:
            raise RuntimeError('Invalid method [%s]' % self.method)
        resp_msg = resp.read()
        log.debug('Response [%s]...', resp_msg)
        resp.close()
        return resp_msg

    @response_wrapper
    def ping(self):
        target_uri = self.server_uri + 'service/ping'
        return self.try_call(target_uri)

    @response_wrapper
    def tokenize(self, params):
        target_uri = self.server_uri + 'secvault/tokenize'
        return self.try_call(target_uri, params)

    @response_wrapper
    def detokenize(self, params):
        target_uri = self.server_uri + 'secvault/detokenize'
        return self.try_call(target_uri, params)

    @response_wrapper
    def search(self, params):
        target_uri = self.server_uri + 'secvault/search'
        return self.try_call(target_uri, params)

    @response_wrapper
    def remove_token(self, params):
        target_uri = self.server_uri + 'secvault/remove_token'
        return self.try_call(target_uri, params)


# vim:ts=4:sts=4:sw=4:tw=85:et:
