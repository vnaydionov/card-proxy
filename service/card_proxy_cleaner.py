# -*- coding: utf-8 -*-

import sys
import os
import logging
import datetime as dt
import xml.etree.ElementTree as et
import MySQLdb


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

    def run(self):
        conn = self.new_db_connection()
        try:
            self.delete_stale_tokens(conn)
        finally:
            conn.close()
            self.logger.info('Disconnected from MySQLdb')

    def delete_stale_tokens(self, conn):
        try:
            cur = conn.cursor()
            q = 'delete from t_data_token where finish_ts >= %s and finish_ts < %s'
            begin_ts = dt.datetime.now() - dt.timedelta(days=10)
            end_ts = dt.datetime.now()
            self.logger.debug('Sql: %s' % (q % (begin_ts, end_ts)))
            cur.execute(q, (begin_ts, end_ts))
            self.logger.info('Commit')
            conn.commit()
        except Exception:
            self.logger.exception('Rollback')
            conn.rollback()

if __name__ == '__main__':
    Application().run()

# vim:ts=4:sts=4:sw=4:et:
