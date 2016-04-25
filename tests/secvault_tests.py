# -*- coding: utf-8 -*-

import os
import sys
import unittest
import datetime as dt

from utils import generate_random_string, generate_random_number
from secvault_api import SecureVaultApi
import logger

log = logger.get_logger('/tmp/secvault_tests-%s.log' % os.environ['USER'])

SERVER_URI = 'http://localhost:17113/'

def log_func_context(func):
    def inner(*args, **kwargs):
        log.debug('---- Start [%s] ----', func.func_name)
        result = func(*args, **kwargs)
        log.debug('---- Start [%s] ----', func.func_name)
        return result
    return inner


class TestSecureVaultApi(unittest.TestCase):
    '''
        tokenize_card, detokenize_card, remove_card, etc.
    '''
    def __init__(self, *args, **kwargs):
        super(TestSecureVaultApi, self).__init__(*args, **kwargs)
        self.server_uri = SERVER_URI

    def _create_new_token(self, plain_text, params=None):
        p = SecureVaultApi(self.server_uri,
                           user='tokenize_only', passwd='qwerty', domain='mydomain')
        new_params = {
            'plain_text': plain_text,
            'expire_ts': (dt.datetime.now() +
                dt.timedelta(days=1500)).isoformat()[:19],
            'dedup': 'false',
            'notify_email': 'someone@somedomain',
        }
        new_params.update(params or {})
        status, resp, f_time = p.tokenize(new_params)
        return resp

    def _unwrap_token(self, token, params=None):
        p = SecureVaultApi(self.server_uri,
                           user='detokenize_only', passwd='qwerty', domain='mydomain')
        new_params = {
            'token': token,
        }
        new_params.update(params or {})
        status, resp, f_time = p.detokenize(new_params)
        return resp

    @log_func_context
    def test_tokenize(self):
        secret = generate_random_string(1000)
        resp = self._create_new_token(secret)
        self.assertEqual('success', resp.findtext('status'))
        self.assertTrue(len(resp.findtext('token')) > 10)

    @log_func_context
    def test_detokenize(self):
        secret = generate_random_string(101)
        resp = self._create_new_token(secret)
        token = resp.findtext('token')
        resp = self._unwrap_token(token + 'xxx')
        self.assertEqual('error', resp.findtext('status'))
        self.assertEqual('token_not_found', resp.findtext('status_code'))
        resp = self._unwrap_token(token)
        self.assertEqual('success', resp.findtext('status'))
        self.assertEqual(secret, resp.findtext('plain_text'))
        self.assertTrue('mydomain', resp.findtext('domain'))
        self.assertTrue(19, len(resp.findtext('expire_ts')))
        self.assertTrue('someone@somedomain', resp.findtext('notify_email'))

    @log_func_context
    def test_no_auth(self):
        secret = generate_random_string(333)
        resp = self._create_new_token(secret, {'user': 'vasya'})
        self.assertEqual('error', resp.findtext('status'))
        self.assertEqual('auth_error', resp.findtext('status_code'))
        resp = self._create_new_token(secret, {'user': 'detokenize_only'})
        self.assertEqual('error', resp.findtext('status'))
        self.assertEqual('auth_error', resp.findtext('status_code'))
        resp = self._create_new_token(secret, {'passwd': 'asdfg'})
        self.assertEqual('error', resp.findtext('status'))
        self.assertEqual('auth_error', resp.findtext('status_code'))
        resp = self._create_new_token(secret, {'domain': 'oebs'})
        self.assertEqual('error', resp.findtext('status'))
        self.assertEqual('auth_error', resp.findtext('status_code'))


if __name__ == '__main__':
    import sys
    sys.argv.append('-v')
    unittest.main()

# vim:ts=4:sts=4:sw=4:tw=85:et:
