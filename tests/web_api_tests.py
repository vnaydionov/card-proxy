# -*- coding: utf-8 -*-

import os
import sys
import unittest

from proxy_web_api import get_resp_field, call_proxy
from utils import generate_random_card_data, generate_random_number
import logger

log = logger.get_logger('web_api_tests.log')

SERVER_URI = 'http://localhost:17117/'

def log_func_context(func):
    def inner(*args, **kwargs):
        log.debug('---- Start [%s] ----', func.func_name)
        result = func(*args, **kwargs)
        log.debug('---- Start [%s] ----', func.func_name)
        return result
    return inner


class TestBaseWebApi(unittest.TestCase):
    '''
        tokenize_card, detokenize_card, remove_card, etc.
    '''
    # TODO add error test
    def __init__(self, *args, **kwargs):
        super(TestBaseWebApi, self).__init__(*args, **kwargs)
        self.server_uri = SERVER_URI

    @log_func_context
    def test_debug_get(self):
        status, resp, f_time = call_proxy(self.server_uri,
                                          'debug_method', 'GET')
        self.assertEqual(status, 'success')

    @log_func_context
    def test_debug_post(self):
        status, resp, f_time = call_proxy(self.server_uri,
                                          'debug_method', 'POST')
        self.assertEqual(status, 'success')

    @log_func_context
    def test_check_kek_get(self):
        status, resp, f_time = call_proxy(self.server_uri,
                                          'check_kek', 'GET')
        self.assertEqual(status, 'success')

    @log_func_context
    def test_dek_status_get(self):
        status, resp, f_time = call_proxy(self.server_uri,
                                          'dek_status', 'GET')
        self.assertEqual(status, 'success')

    @log_func_context
    def test_get_token_with_cvn_get(self):
        card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'GET', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'cvn_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))
        self.assertFalse(get_resp_field(resp, 'cvn'))

    @log_func_context
    def test_get_token_with_cvn_post(self):
        card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'POST', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'cvn_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))
        self.assertFalse(get_resp_field(resp, 'cvn'))

    @log_func_context
    def test_get_token_without_cvn_get(self):
        card_data = generate_random_card_data(mode='without_cvn')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'GET', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))

    @log_func_context
    def test_get_token_without_cvn_post(self):
        card_data = generate_random_card_data(mode='without_cvn')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'POST', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))

    @log_func_context
    def test_get_token_multiple_duplicate_get(self):
        card_data = generate_random_card_data(mode='full')
        orig_pan = card_data.pan
        orig_cvn = card_data.cvn
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'GET', card_data)
        orig_card_token = get_resp_field(resp, 'card_token')
        orig_cvn_token = get_resp_field(resp, 'cvn_token')
        for _ in range(5):
            card_data.pan = orig_pan
            card_data.cvn = orig_cvn
            status, resp, f_time = call_proxy(self.server_uri,
                                              'tokenize_card', 'GET', card_data)
            dup_card_token = get_resp_field(resp, 'card_token')
            dup_cvn_token = get_resp_field(resp, 'cvn_token')
            self.assertEqual(orig_card_token, dup_card_token)
            self.assertNotEqual(orig_cvn_token, dup_cvn_token)

    @log_func_context
    def test_get_token_multiple_duplicate_post(self):
        card_data = generate_random_card_data(mode='full')
        orig_pan = card_data.pan
        orig_cvn = card_data.cvn
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'POST', card_data)
        orig_card_token = get_resp_field(resp, 'card_token')
        orig_cvn_token = get_resp_field(resp, 'cvn_token')
        for _ in range(5):
            card_data.pan = orig_pan
            card_data.cvn = orig_cvn
            status, resp, f_time = call_proxy(self.server_uri,
                                              'tokenize_card', 'POST', card_data)
            dup_card_token = get_resp_field(resp, 'card_token')
            dup_cvn_token = get_resp_field(resp, 'cvn_token')
            self.assertEqual(orig_card_token, dup_card_token)
            self.assertNotEqual(orig_cvn_token, dup_cvn_token)

    @log_func_context
    def test_get_card_get(self):
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'GET', source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'detokenize_card', 'GET',
            card_token, cvn_token)
        self.assertEqual(status, 'success')
        self.assertEqual(source_card_data.pan, get_resp_field(resp, 'pan'))
        self.assertEqual(source_card_data.cvn, get_resp_field(resp, 'cvn'))
        # self.assertEqual(int(source_card_data.expire_year),
        #                  int(get_resp_field(resp, 'expire_year')))
        # self.assertEqual(int(source_card_data.expire_month),
        #                  int(get_resp_field(resp, 'expire_month')))

    @log_func_context
    def test_get_card_post(self):
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'POST',
                                          source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'detokenize_card', 'POST',
            card_token, cvn_token)
        self.assertEqual(status, 'success')
        self.assertEqual(source_card_data.pan, get_resp_field(resp, 'pan'))
        self.assertEqual(source_card_data.cvn, get_resp_field(resp, 'cvn'))
        # self.assertEqual(int(source_card_data.expire_year),
        #                  int(get_resp_field(resp, 'expire_year')))
        # self.assertEqual(int(source_card_data.expire_month),
        #                  int(get_resp_field(resp, 'expire_month')))

    @log_func_context
    def test_remove_card_get(self):
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'GET',
                                          source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'remove_card', 'GET',
            card_token, cvn_token)
        self.assertEqual(status, 'success')

    @log_func_context
    def test_remove_card_post(self):
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'tokenize_card', 'POST',
                                          source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'remove_card', 'POST',
            card_token, cvn_token)
        self.assertEqual(status, 'success')


if __name__ == '__main__':
    import sys
    sys.argv.append('-v')
    unittest.main()

# vim:ts=4:sts=4:sw=4:tw=85:et:
