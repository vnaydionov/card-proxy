# -*- coding: utf-8 -*-

import os
import sys
import unittest
import random

from proxy_web_api import get_resp_field, call_proxy, generate_random_card_data, \
        generate_random_number
import logger

log = logger.get_logger()

SERVER_URI = 'http://localhost:17117/debug_api'


class TestBaseWebApi(unittest.TestCase):
    '''
        get_token, get_card, remove_card, etc.
    '''
    # TODO add error test
    def __init__(self, *args, **kwargs):
        super(TestBaseWebApi, self).__init__(*args, **kwargs)
        self.server_uri = SERVER_URI

    def test_debug_get(self):
        log.debug('Starting test_debug_get...')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'debug_method', 'GET')
        self.assertEqual(status, 'success')

    def test_debug_post(self):
        log.debug('Starting test_debug_post...')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'debug_method', 'POST')
        self.assertEqual(status, 'success')

    def test_dek_status_get(self):
        log.debug('Starting test_dek_status_get...')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'dek_status', 'GET')
        self.assertEqual(status, 'success')

    def test_dek_status_post(self):
        log.debug('Starting test_dek_status_post...')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'dek_status', 'POST')
        self.assertEqual(status, 'success')

    def test_get_token_with_cvn_get(self):
        log.debug('Starting test_get_token_with_cvn_get...')
        card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'GET', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'cvn_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))
        self.assertFalse(get_resp_field(resp, 'cvn'))

    def test_get_token_with_cvn_post(self):
        log.debug('Starting test_get_token_with_cvn_post...')
        card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'POST', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'cvn_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))
        self.assertFalse(get_resp_field(resp, 'cvn'))

    def test_get_token_without_cvn_get(self):
        log.debug('Starting test_get_token_without_cvn_get...')
        card_data = generate_random_card_data(mode='without_cvn')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'GET', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))

    def test_get_token_without_cvn_post(self):
        log.debug('Starting test_get_token_without_cvn_get_post...')
        card_data = generate_random_card_data(mode='without_cvn')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'POST', card_data)
        self.assertEqual(status, 'success')
        self.assertTrue(get_resp_field(resp, 'card_token'))
        self.assertTrue(get_resp_field(resp, 'pan_masked'))
        self.assertFalse(get_resp_field(resp, 'pan'))

    def test_get_token_multiple_duplicate_get(self):
        log.debug('Starting test_get_token_multiple_duplicate_get...')
        card_data = generate_random_card_data(mode='without_cvn')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'GET', card_data)
        orig_pan = card_data.pan
        orig_card_token = get_resp_field(resp, 'card_token')
        for _ in range(10):
            new_pan = orig_pan[:6] + generate_random_number(6) \
                + orig_pan[12:]
            card_data.pan = new_pan
            status, resp, f_time = call_proxy(self.server_uri,
                                              'get_token', 'GET', card_data)
            dup_card_token = get_resp_field(resp, 'card_token')
            self.assertTrue(orig_card_token != dup_card_token)
        card_data.pan = orig_pan
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'GET', card_data)
        orig_dup_card_token = get_resp_field(resp, 'card_token')
        self.assertEqual(orig_card_token, orig_dup_card_token)

    def test_get_token_multiple_duplicate_post(self):
        log.debug('Starting test_get_token_multiple_duplicate_post...')
        card_data = generate_random_card_data(mode='without_cvn')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'POST', card_data)
        orig_pan = card_data.pan
        orig_card_token = get_resp_field(resp, 'card_token')
        for _ in range(10):
            new_pan = orig_pan[:6] + generate_random_number(6) \
                + orig_pan[12:]
            card_data.pan = new_pan
            status, resp, f_time = call_proxy(self.server_uri,
                                              'get_token', 'POST', card_data)
            dup_card_token = get_resp_field(resp, 'card_token')
            self.assertTrue(orig_card_token != dup_card_token)
        card_data.pan = orig_pan
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'POST', card_data)
        orig_dup_card_token = get_resp_field(resp, 'card_token')
        self.assertEqual(orig_card_token, orig_dup_card_token)

    def test_get_card_get(self):
        log.debug('Starting test_get_card_get...')
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'GET', source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'get_card', 'GET',
            card_token, cvn_token)
        self.assertEqual(status, 'success')
        self.assertEqual(source_card_data.pan, get_resp_field(resp, 'pan'))
        self.assertEqual(source_card_data.cvn, get_resp_field(resp, 'cvn'))
##        self.assertEqual(str(source_card_data.card_holder),
##                         get_resp_field(resp, 'card_holder'))
##        self.assertEqual(int(source_card_data.expire_year),
##                         int(get_resp_field(resp, 'expire_year')))
##        self.assertEqual(int(source_card_data.expire_month),
##                         int(get_resp_field(resp, 'expire_month')))

    def test_get_card_post(self):
        log.debug('Starting test_get_card_post...')
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'POST',
                                          source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'get_card', 'POST',
            card_token, cvn_token)
        self.assertEqual(status, 'success')
        self.assertEqual(source_card_data.pan, get_resp_field(resp, 'pan'))
        self.assertEqual(source_card_data.cvn, get_resp_field(resp, 'cvn'))
##        self.assertEqual(str(source_card_data.card_holder),
##                         get_resp_field(resp, 'card_holder'))
##        self.assertEqual(int(source_card_data.expire_year),
##                         int(get_resp_field(resp, 'expire_year')))
##        self.assertEqual(int(source_card_data.expire_month),
##                         int(get_resp_field(resp, 'expire_month')))

    def test_remove_card_get(self):
        log.debug('Starting test_remove_card_get...')
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'GET',
                                          source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'remove_card_data', 'GET',
            card_token, cvn_token)
        self.assertEqual(status, 'success')

    def test_remove_card_post(self):
        log.debug('Starting test_remove_card_post...')
        source_card_data = generate_random_card_data(mode='full')
        status, resp, f_time = call_proxy(self.server_uri,
                                          'get_token', 'POST',
                                          source_card_data)
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        status, resp, f_time = call_proxy(
            self.server_uri, 'remove_card_data', 'POST',
            card_token, cvn_token)
        self.assertEqual(status, 'success')


if __name__ == '__main__':
    import sys
    sys.argv.append('-v')
    unittest.main()

# vim:ts=4:sts=4:sw=4:tw=85:et:
