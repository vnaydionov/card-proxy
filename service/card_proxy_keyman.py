#!/usr/bin/python

import sys
import os
import time
import pprint
from optparse import OptionParser
import urllib2
import xml.etree.ElementTree as et
import subprocess


def encode_params(params):
    return '&'.join(['%s=%s' % (urllib2.quote(str(k)), urllib2.quote(str(v)))
                     for (k, v) in params.iteritems()])


class Application(object):

    def _parse_params(self):
        commands = (
            'status',
            'generate_kek',
            'generate_hmac',
            'get_component',
            'confirm_component',
            'switch_kek',
            'switch_hmac',
            'status_monitor',
            'reset_target_version',
            'cleanup',
        )

        parser = OptionParser(
            description='''
Possible commands: [%s]
Basic workflow: generate_kek -> get_component x3 -> confirm_component x3 -> check_status -> switch_kek -> cleanup
''' % ', '.join(c for c in commands)
        )
        parser.add_option('-H', '--host', dest='host',
            help='host for usage (current host by default)')
        parser.add_option('-k', '--key-version', dest='key_version',
            help='operate on the version specified')
        (options, args) = parser.parse_args()

        if len(args) != 1 or args[0] not in commands:
            parser.print_usage()
            sys.exit(1)
        else:
            self.COMMAND = args[0]

        self.HOST = 'localhost'
        if options.host:
            self.HOST = options.host

        self.KEY_VERSION = None
        if options.key_version is not None:
            self.KEY_VERSION = options.key_version

        self.CLEAR_TIMEOUT = 30.0

    def __init__(self):
        self._parse_params()

    def call_keyapi(self, method, **kwargs):
        uri = 'https://%s:15119/keyapi/' % self.HOST
        #print '%s %s' % (uri + method, encode_params(kwargs or {}))
        f = urllib2.urlopen(uri + method, encode_params(kwargs or {}))
        data = f.read()
        f.close()
        resp = et.XML(data)
        result = {}
        for item in resp.findall('*'):
            if item.tag == 'master_keys':
                for sub_item in item.findall('kek'):
                    state = ''
                    if sub_item.attrib['checked'] == 'true':
                        state += 'checked '
                    if sub_item.attrib['valid'] == 'true':
                        state += 'valid '
                    result['kek_status_%s' % sub_item.attrib['version']] = state
                    result['kek_count_%s' % sub_item.attrib['version']] = \
                        sub_item.attrib['count']
            if item.tag == 'hmac_keys':
                for sub_item in item.findall('hmac'):
                    state = 'valid'
                    result['hmac_status_%s' % sub_item.attrib['version']] = state
                    result['hmac_count_%s' % sub_item.attrib['version']] = \
                        sub_item.attrib['count']
            if item.tag not in ('purged_keys', 'master_keys', 'hmac_keys'):
                result[item.tag] = item.text
        return result

    def process_response(self, resp, clear=False):
        if resp.get('status') != 'success':
            print "SOMETHING WENT WRONG"
            print pprint.pprint(resp)
        else:
            table = []
            for (key, value) in resp.iteritems():
                if key != 'STATE_EXTENSION':
                    table.append((key, value))
                else:
                    data = json.loads(value)
                    for k, v in data.items():
                        table.append((k, v))
            table.sort(key=lambda row: row[0])

            if clear:
                subprocess.call('clear')
            col1_width = 25
            col2_width = 45
            print '=' * (col1_width + col2_width + 3*3)
            fmt = '|| %%-%ds | %%-%ds ||' % (col1_width, col2_width)
            for cell1, cell2 in [('VAR', 'VALUE')] + table:
                print fmt % (cell1, cell2)
            print '=' * (col1_width + col2_width + 3*3)

    def process_command(self):
        messages = {
            'generate_kek': 'A new KEK was just created and distributed, '
                'use "get_component"/"confirm_component" for retrieval/confirmation '
                'for each of three parts, after that use "switch_kek"; '
                'do not forget about "generate_hmac" and "switch_hmac" too.',
            'generate_hmac': 'New HMAC key was just created, now use "switch_hmac".',
            'get_component': 'Now proceed to "confirm_component".',
            'confirm_component': 'If all three components were confirmed, '
                'reencryption will start automatically and you can do "switch_kek".',
            'switch_kek': 'KEK was just switched, don\'t forget "cleanup" after reencryption.',
            'switch_hmac': 'HMAC key was just switched, don\'t forget "cleanup" after rehashing.',
            'reset_target_version': 'KEK target version was just switched.',
        }

        if self.COMMAND == 'status_monitor':
            while True:
                resp = self.call_keyapi('status')
                self.process_response(resp, clear=True)
                time.sleep(2)

        elif self.COMMAND == 'status':
            resp = self.call_keyapi('status')
            self.process_response(resp)

        else:
            component = raw_input('Type your component: ').strip()
            kwargs = {'password': component}
            if self.KEY_VERSION is not None:
                kwargs['version'] = self.KEY_VERSION
            resp = self.call_keyapi(self.COMMAND, **kwargs)
            self.process_response(resp)

            if self.COMMAND in messages:
                print messages[self.COMMAND]
            print 'terminal will be cleared in %s seconds' % self.CLEAR_TIMEOUT
            time.sleep(self.CLEAR_TIMEOUT)
            subprocess.call('reset')

    def run(self):
        try:
            self.process_command()
        except KeyboardInterrupt:
            print 'Interrupted'

if __name__ == '__main__':
    Application().run()

# vim:ts=4:sts=4:sw=4:et:
