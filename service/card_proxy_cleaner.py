# -*- coding: utf-8 -*-

import datetime as dt
from application import Application


class CleanerApp(Application):
    app_name = 'card_proxy_cleaner'

    def on_run(self):
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
    CleanerApp().run()

# vim:ts=4:sts=4:sw=4:et:
