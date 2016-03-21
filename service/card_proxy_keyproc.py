# -*- coding: utf-8 -*-

import sys
import random
import xml.etree.ElementTree as et
import urllib2
from application import Application, calculate_batches


class KeyProcApp(Application):
    app_name = 'card_proxy_keyproc'

    def _parse_params(self):
        super(KeyProcApp, self)._parse_params()
        self.SHUFFLE_BATCHES = False
        if '-shuffle-batches' in sys.argv:
            self.SHUFFLE_BATCHES = True

    def on_run(self):
        self.reencrypt_deks()

    def load_config_params(self, conn):
        rows = self.do_select(conn, 'select ckey, cvalue from t_config where ckey like %s',
                              ('KEK_%',))
        self.db_config = dict(rows)

    def call_keyapi(self, method, **kwargs):
        resp = self.call_xml_api('KeyAPI', method, **kwargs)
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
                self.db_config.get('KEK_TARGET_VERSION') or 0)
            self.logger.info('Target KEK: %s', target_version)
            resp = self.call_keyapi('status')
            target_status = resp.get('kek_status_%s' % target_version) or ''
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
                return
            return (target_version, min_id, max_id)
        finally:
            self.put_db_connection(conn)

    def process_batch(self, batch_min_id, batch_max_id):
        self.logger.info('Reencrypt batch: [%s .. %s]',
            batch_min_id, batch_max_id)
        resp = self.call_keyapi('reencrypt_deks',
            id_min=batch_min_id, id_max=batch_max_id)
        converted_cnt = int(resp.get('converted') or 0)
        failed_cnt = int(resp.get('failed') or 0)
        return (converted_cnt, failed_cnt)

    def check_target_version(self, target_version0):
        conn = self.new_db_connection()
        self.load_config_params(conn)
        self.put_db_connection(conn)
        target_version = int(
            self.db_config.get('KEK_TARGET_VERSION') or 0)
        if target_version != target_version0:
            self.logger.warning('Target KEK changed: %s -> %s',
                target_version0, target_version)
            return False
        return True

    def reencrypt_deks(self):
        batch_size = int(self.cfg.findtext('Reencrypt/BatchSize'))
        time_limit = int(self.cfg.findtext('Reencrypt/TimeLimit'))
        task = self.get_reencryption_task()
        if task is None:
            self.logger.info('Nothing to do')
            return
        (target_version, min_id, max_id) = task
        batches = calculate_batches(min_id, max_id, batch_size, self.SHUFFLE_BATCHES)
        message = 'Rehashing done'
        target_version0 = target_version
        converted_cnt = 0
        failed_cnt = 0
        for (batch_min_id, batch_max_id) in batches:
            (batch_converted_cnt, batch_failed_cnt) = \
                self.process_batch(batch_min_id, batch_max_id)
            converted_cnt += batch_converted_cnt
            failed_cnt += batch_failed_cnt
            if not self.check_target_version(target_version0):
                message = 'Target changed'
                break
            if self.get_elapsed_time() >= time_limit:
                message = 'Time limit reached'
                break
        self.logger.info('%s! Converted: %d, failed %d',
                         message, converted_cnt, failed_cnt)


if __name__ == '__main__':
    KeyProcApp().run()

# vim:ts=4:sts=4:sw=4:et:
