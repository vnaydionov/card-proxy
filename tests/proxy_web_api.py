# -*- coding: utf-8 -*-

import time
import urllib2
import urllib
import re
import random
import os

import logger

log = logger.get_logger()


def generate_random_number(length):
    def byte_to_int(byte):
        return '%i' % (ord(byte) * 10 // 256)
    rand_seq = os.urandom(length)
    result = ''.join([byte_to_int(elem) for elem in rand_seq])
    return result


def generate_random_string(length):
    def byte_to_char(byte):
        val = ord(byte) * (26*2 + 10) // 256
        if val < 26:
            return chr(ord('A') + val)
        elif val < 26*2:
            return chr(ord('a') + (val - 26))
        else:
            return chr(ord('0') + (val - 26*2))
    rand_seq = os.urandom(length)
    result = ''.join([byte_to_char(elem) for elem in rand_seq])
    return result


class CardData(object):
    def __init__(self, **kwargs):
        for k, v in kwargs.iteritems():
            setattr(self, k, v)

    def to_dict(self):
        return dict([
            (k, getattr(self, k)) for k in dir(self)
            if not (k.startswith('_') or callable(getattr(self, k)))
            ])


def generate_random_card_data(mode='full', pan_len=16):
    if mode == 'full':
        card_data = CardData(
            pan=generate_random_number(16),
            expire_year=random.randrange(2016, 2030),
            expire_month=random.randrange(1, 13),
            card_holder=generate_random_string(20),
            cvn=generate_random_number(3)
        )
    elif mode == 'without_cvn':
        card_data = CardData(
            pan=generate_random_number(16),
            expire_year=random.randrange(2016, 2030),
            expire_month=random.randrange(1, 13),
            card_holder=generate_random_string(20),
        )
    elif mode == 'only_cvn':
        card_data = CardData(
            cvn=generate_random_number(3)
        )
    else:
        raise RuntimeError('Invalid mode [%s]' % mode)
    return card_data


def response_wrapper(f):
    def time_f(*args):
        s_time = time.time()
        response = f(*args)
        f_time = time.time()
        status = get_status(response)
        return status, response, f_time - s_time
    return time_f


class CardProxyWebApi(object):
    def __init__(self, server_uri, timeout=2.0, method='POST'):
        self.server_uri = server_uri
        self.timeout = timeout
        self.method = method

    def try_call(self, uri, user_data=None):
        log.debug('Connect to [%s]...', uri)
        log.debug('Data: [%s]', repr(user_data))
        if self.method == 'POST':
            if not user_data:
                user_data = 'random_trash_for_post'
            resp = urllib2.urlopen(uri, user_data, timeout=self.timeout)
        elif self.method == 'GET':
            if user_data:
                uri += '?' + user_data
            resp = urllib2.urlopen(uri, timeout=self.timeout)
        else:
            raise RuntimeError('Invalid method [%s]' % self.method)
        resp_msg = resp.read()
        log.debug('Response [%s]...', resp_msg)
        resp.close()
        return resp_msg

    @response_wrapper
    def debug_method(self):
        target_uri = self.server_uri + 'debug_api/debug_method'
        return self.try_call(target_uri)

    @response_wrapper
    def dek_status(self):
        target_uri = self.server_uri + 'debug_api/dek_status'
        return self.try_call(target_uri)

    @response_wrapper
    def check_kek(self):
        target_uri = self.server_uri + 'ping/check_kek'
        return self.try_call(target_uri)

    @response_wrapper
    def tokenize_card(self, card_data):
        uri_params = urllib.urlencode(card_data.to_dict())
        target_uri = self.server_uri + 'debug_api/tokenize_card'
        return self.try_call(target_uri, uri_params)

    @response_wrapper
    def detokenize_card(self, card_token=None, cvn_token=None):
        params = {}
        if card_token:
            params['card_token'] = card_token
        if cvn_token:
            params['cvn_token'] = cvn_token
        if not params.items():
            raise RuntimeError()
        uri_params = urllib.urlencode(params)
        target_uri = self.server_uri + 'debug_api/detokenize_card'
        return self.try_call(target_uri, uri_params)

    @response_wrapper
    def remove_card(self, card_token=None, cvn_token=None):
        params = {}
        if card_token:
            params['card_token'] = card_token
        if cvn_token:
            params['cvn_token'] = cvn_token
        if not params.items():
            raise RuntimeError()
        uri_params = urllib.urlencode(params)
        target_uri = self.server_uri + 'debug_api/remove_card'
        return self.try_call(target_uri, uri_params)


def call_proxy(server_uri, method_name, http_method, *params):
    web_api = CardProxyWebApi(server_uri, method=http_method)
    if hasattr(web_api, method_name):
        api_method = getattr(web_api, method_name)
    else:
        log.debug('Method not found in CardProxyWebApi: [%s]', method_name)
        return
    log.debug('Calling [CardProxy.%s] with params [%s]...',
              method_name, params)
    response = api_method(*params)
    log.debug('Response from [CardProxy.%s]: [%s]',
              method_name, repr(response))
    return response


def get_status(resp):
    return get_resp_field(resp, 'status', 'error')


def get_status_code(resp):
    return get_resp_field(resp, 'status_code', 'error')


def get_resp_field(resp, name, default=None):
    field_pattern = r'<{0}>([\w\d*]*)</{0}>'.format(name)
    field_str = re.search(field_pattern, resp)
    if field_str and field_str.groups():
        return field_str.groups()[0]
    return default

# vim:ts=4:sts=4:sw=4:tw=85:et:
