# -*- coding: utf-8 -*-

import sys
import random
import xml.etree.ElementTree as et
import urllib2
from application import Application, calculate_batches


class HmacProcApp(Application):
    app_name = 'card_proxy_hmacproc'

    def _parse_params(self):
        super(HmacProcApp, self)._parse_params()
        self.SHUFFLE_BATCHES = False
        if '-shuffle-batches' in sys.argv:
            self.SHUFFLE_BATCHES = True

    def on_run(self):
        self.rehash_tokens()

    def load_config_params(self, conn):
        rows = self.do_select(conn, 'select ckey, cvalue from t_config where ckey like %s',
                              ('HMAC_%',))
        self.db_config = dict(rows)

    def call_keyapi(self, method, **kwargs):
        resp = self.call_xml_api('KeyAPI', method, **kwargs)
        result = {}
        for item in resp.findall('*'):
            if item.tag == 'hmac_keys':
                for sub_item in item.findall('hmac'):
                    state = 'valid'
                    result['hmac_status_%s' % sub_item.attrib['version']] = state
            if item.tag not in ('purged_keys', 'master_keys', 'hmac_keys'):
                result[item.tag] = item.text
        return result

    def get_rehashing_task(self):
        conn = self.new_db_connection()
        try:
            self.load_config_params(conn)
            target_version = int(
                self.db_config.get('HMAC_VERSION') or 0)
            self.logger.info('Target HMAC: %s', target_version)
            resp = self.call_keyapi('status')
            target_status = resp.get('hmac_status_%s' % target_version)
            if 'valid' not in target_status:
                self.logger.warning('Target HMAC is not valid')
                return None
            rows = self.do_select(conn,
                'select min(id), max(id) from t_data_token where hmac_version <> %s',
                (target_version,))
            min_id, max_id = rows[0]
            if min_id is None:
                return
            return (target_version, min_id, max_id)
        finally:
            self.put_db_connection(conn)

    def process_batch(self, batch_min_id, batch_max_id):
        self.logger.info('Rehash batch: [%s .. %s]',
            batch_min_id, batch_max_id)
        resp = self.call_keyapi('rehash_tokens',
            id_min=batch_min_id, id_max=batch_max_id)
        converted_cnt = int(resp.get('converted') or 0)
        failed_cnt = int(resp.get('failed') or 0)
        return (converted_cnt, failed_cnt)

    def check_target_version(self, target_version0):
        conn = self.new_db_connection()
        self.load_config_params(conn)
        self.put_db_connection(conn)
        target_version = int(
            self.db_config.get('HMAC_VERSION') or 0)
        if target_version != target_version0:
            self.logger.warning('Target HMAC changed: %s -> %s',
                target_version0, target_version)
            return False
        return True

    def rehash_tokens(self):
        batch_size = int(self.cfg.findtext('Rehash/BatchSize'))
        time_limit = int(self.cfg.findtext('Rehash/TimeLimit'))
        task = self.get_rehashing_task()
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
    HmacProcApp().run()

# vim:ts=4:sts=4:sw=4:et:
