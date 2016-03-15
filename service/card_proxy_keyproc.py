# -*- coding: utf-8 -*-

import sys
import os
import logging
import urllib2
import datetime as dt
import xml.etree.ElementTree as et
import MySQLdb


def encode_params(params):
    return '&'.join(['%s=%s' % (urllib2.quote(str(k)), urllib2.quote(str(v)))
                     for (k, v) in params.iteritems()])


class Application(object):
    def __init__(self):
        self.cfg = et.parse(os.environ['CONFIG_FILE'])
        log_file = self.cfg.findtext('Log')
        logging.basicConfig(
            filename=log_file, level=logging.DEBUG,
            format='%(asctime)s %(name)-12s %(levelname)-8s %(message)s')
        logger = logging.getLogger('')
        logger.setLevel(logging.DEBUG)
        logger = logging.getLogger('card_proxy_cleaner.%d' % os.getpid())
        logger.setLevel(logging.DEBUG)
        logger.info('Application started')
        self.logger = logger

    def new_db_connection(self):
        try:
            host = self.cfg.findtext('DbBackend/Host')
            port = int(self.cfg.findtext('DbBackend/Port'))
            database = self.cfg.findtext('DbBackend/DB')
            user = self.cfg.findtext('DbBackend/User')
            password = self.cfg.findtext('DbBackend/Pass')
            connection = MySQLdb.connect(user=user, passwd=password,
                                         host=host, port=port, db=database)
            self.logger.info('Connected to MySQLdb: %s@%s:%d/%s' % (user, host, port, database))
            return connection
        except Exception:
            self.logger.exception('Connect failed')
            raise

    def put_db_connection(self, conn):
        conn.close()
        self.logger.info('Disconnected from MySQLdb')

    def run(self):
        try:
            self.reencrypt_deks()
        except Exception:
            self.logger.exception('')

    def do_select(self, conn, q, params):
        self.logger.info('SQL: %s', q)
        self.logger.debug('Params: %r', params)
        cur = conn.cursor()
        cur.execute(q, params)
        rows = [row for row in cur.fetchall()]
        self.logger.debug('Rows:\n%r', rows)
        return rows

    def load_config_params(self, conn):
        rows = self.do_select(conn, 'select ckey, cvalue from t_config where ckey like %s',
                              ('KEK_%',))
        self.db_config = dict(rows)
    
    def call_keyapi(self, method, **kwargs):
        uri = self.cfg.findtext('KeyAPI/URL')
        self.logger.info('Calling KeyAPI: %s params: %r', uri + method, kwargs)
        f = urllib2.urlopen(uri + method, encode_params(kwargs or {}))
        data = f.read()
        f.close()
        self.logger.info('Response from KeyAPI: %s', data)
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
            if item.tag not in ('purged_keys', 'master_keys', 'hmac_keys'):
                result[item.tag] = item.text
        return result

    def get_reencryption_task(self):
        conn = self.new_db_connection()
        try:
            self.load_config_params(conn)
            target_version = int(
                self.db_config.get('KEK_TARGET_VERSION') or -1)
            self.logger.info('Target KEK: %s', target_version)
            resp = self.call_keyapi('status')
            target_status = resp.get('kek_status_%s' % target_version)
            if 'valid' not in target_status:
                self.logger.warning('Target KEK is not valid')
                return None
            if 'checked' not in target_status:
                self.logger.warning('Target KEK is not checked')
                return None
            rows = self.do_select(conn,
                'select min(id), max(id) from t_dek where kek_version <> %s',
                (target_version,))
            min_id, max_id = rows[0]
            if min_id is None:
                self.logger.info('Nothing to do')
                return
            return (target_version, min_id, max_id)
        finally:
            self.put_db_connection(conn)

    def reencrypt_deks(self):
        batch_size = int(self.cfg.findtext('Reencrypt/BatchSize'))
        task = self.get_reencryption_task()
        if task is None:
            return
        (target_version, min_id, max_id) = task
        target_version0 = target_version
        total_cnt = max_id - min_id + 1
        batch_cnt = (total_cnt + batch_size - 1) / batch_size
        failed = False
        for i in range(batch_cnt):
            batch_min_id = i * batch_size
            batch_max_id = batch_min_id + batch_size - 1
            self.logger.info('Reencrypt batch: [%s .. %s]',
                batch_min_id, batch_max_id)
            resp = self.call_keyapi('reencrypt_deks',
                id_min=batch_min_id, id_max=batch_max_id)
            conn = self.new_db_connection()
            self.load_config_params(conn)
            self.put_db_connection(conn)
            target_version = int(
                self.db_config.get('KEK_TARGET_VERSION') or -1)
            if target_version != target_version0:
                self.logger.warning('Target KEK changed: %s -> %s',
                    target_version0, target_version)
                failed = True
                break
        if not failed:
            self.logger.info('Reencryption done!')

if __name__ == '__main__':
    Application().run()

# vim:ts=4:sts=4:sw=4:et:
