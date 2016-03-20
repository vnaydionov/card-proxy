# -*- coding: utf-8 -*-

import sys
import os
import random
import time
import xml.etree.ElementTree as et
import logging
from logging import Formatter, FileHandler
from logging.handlers import SysLogHandler
import urllib2


class Application(object):
    app_name = 'app_name'

    def __init__(self):
        self._parse_params()
        self.cfg = et.parse(os.environ['CONFIG_FILE'])
        self._init_logger()
        if self.RANDOM_DELAY:
            delay = random.random() * 30
            self.logger.info('Random delay: %.3f sec', delay)
            time.sleep(delay)
        self.logger.info('Application started')
        self.start_ts = time.time()

    def get_elapsed_time(self):
        return time.time() - self.start_ts

    def _parse_params(self):
        self.RANDOM_DELAY = False

        if '-random-delay' in sys.argv:
            self.RANDOM_DELAY = True

    def _init_logger(self):
        root_logger = logging.getLogger('')
        root_logger.setLevel(logging.DEBUG)
        log_file = self.cfg.findtext('Log')
        if log_file == 'syslog':
            handler = SysLogHandler(address='/dev/log')
            program = '%s[%d]' % (self.app_name, os.getpid())
            fmt_str = '%s: %%(name)-12s: %%(message)s' % program
            formatter = Formatter(fmt_str)
        else:
            handler = FileHandler(log_file)
            formatter = Formatter(
                '%(asctime)s P%(process)-6d %(name)-12s %(levelname)-6s %(message)s')
        handler.setFormatter(formatter)
        root_logger.addHandler(handler)
        logger = logging.getLogger('%s' % self.app_name)
        logger.setLevel(logging.DEBUG)
        self.logger = logger

    def new_db_connection(self):
        try:
            import MySQLdb
            host = self.cfg.findtext('DbBackend/Host')
            port = int(self.cfg.findtext('DbBackend/Port'))
            database = self.cfg.findtext('DbBackend/DB')
            user = self.cfg.findtext('DbBackend/User')
            password = self.cfg.findtext('DbBackend/Pass')
            connection = MySQLdb.connect(user=user, passwd=password,
                                         host=host, port=port, db=database)
            self.logger.info('Connected to MySQLdb: %s@%s:%d/%s',
                             user, host, port, database)
            return connection
        except Exception:
            self.logger.exception('Connect failed')
            raise

    def do_select(self, conn, q, params):
        self.logger.info('SQL: %s', q)
        self.logger.debug('Params: %r', params)
        cur = conn.cursor()
        cur.execute(q, params)
        rows = [row for row in cur.fetchall()]
        cur.close()
        self.logger.debug('Rows:\n%r', rows)
        return rows

    def put_db_connection(self, conn):
        conn.close()
        self.logger.info('Disconnected from MySQLdb')

    def call_xml_api(self, api, method, **kwargs):
        uri = self.cfg.findtext('%s/URL' % api)
        if not uri.endswith('/'):
            uri += '/'
        uri += method
        self.logger.info('Calling %s: %s', api, uri)
        self.logger.debug('params: %r', kwargs)
        f = urllib2.urlopen(uri, encode_params(kwargs or {}))
        data = f.read()
        f.close()
        self.logger.debug('Response from %s: %s', api, data)
        resp = et.XML(data)
        return resp

    def run(self):
        try:
            self.on_run()
        except Exception:
            self.logger.exception('')
        self.logger.info('Application finished. %.3f sec elapsed.',
                         self.get_elapsed_time())


def encode_params(params):
    return '&'.join(['%s=%s' % (urllib2.quote(str(k)), urllib2.quote(str(v)))
                     for (k, v) in params.iteritems()])


def calculate_batches(min_id, max_id, batch_size, shuffle):
    total_range = max_id - min_id + 1
    batch_cnt = (total_range + batch_size - 1) / batch_size
    batches = [(min_id + i * batch_size,
                min_id + (i + 1) * batch_size - 1)
               for i in range(batch_cnt)]
    if shuffle:
        random.shuffle(batches)
    return batches

# vim:ts=4:sts=4:sw=4:et:
