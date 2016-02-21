# -*- coding: utf-8 -*-

import os
import sys
import time
import random
import math
from bisect import insort
from multiprocessing import Process, Queue, Event
from Queue import Empty

from utils import generate_random_card_data
from proxy_web_api import get_resp_field, call_proxy
import logger

log = logger.get_logger()


HTTP_METHOD = 'POST'
SERVER_URI = 'http://localhost:17117/'


def wrapped_call_proxy(method, *args):
    return call_proxy(SERVER_URI, method, HTTP_METHOD, *args)


class ApiMethodStat(object):
    def __init__(self, method_name):
        self.method_name = method_name
        self.exec_times = []
        self.errors = {}
        self.total_calls = 0
        self.success_calls = 0
        self.failure_calls = 0
        self.min_exec_time = 0.0
        self.max_exec_time = 0.0
        self.avg_exec_time = 0.0
        self.exec_time_percentiles = {
            '75': 0.0,
            '90': 0.0,
            '95': 0.0,
            '99': 0.0,
        }

    def calc_stat(self):
        if self.exec_times:
            self.min_exec_time = min(self.exec_times)
            self.max_exec_time = max(self.exec_times)
            self.avg_exec_time = avg(self.exec_times)
            for percent in self.exec_time_percentiles:
                value = percentile(self.exec_times, float(percent) / 100)
                self.exec_time_percentiles[percent] = value

    def add(self, params):
        self.total_calls += 1
        status = params.get('status')
        if status == 'success':
            self.success_calls += 1
            exec_time = params.get('exec_time')
            insort(self.exec_times, exec_time)
        else:
            log.debug('ERROR [%s]', repr(params))
            self.failure_calls += 1
            resp = params['response']
            self.errors[resp] = self.errors.get(resp, 0) + 1

    def get_stat(self):
        self.calc_stat()
        return {
            'name': self.method_name,
            'total': self.total_calls,
            'min': self.min_exec_time,
            'max': self.max_exec_time,
            'avg': self.avg_exec_time,
            '75p': self.exec_time_percentiles['75'],
            '90p': self.exec_time_percentiles['90'],
            '95p': self.exec_time_percentiles['95'],
            '99p': self.exec_time_percentiles['99'],
            'errors': self.errors,
        }


def avg(num_list):
    return sum(num_list) / float(len(num_list))


def percentile(s_list, percent):
    if not s_list:
        return None
    k_id = (len(s_list) - 1) * percent
    f, c = math.floor(k_id), math.ceil(k_id)
    if f == c:
        return s_list[int(k_id)]
    return s_list[int(f)] * (c - k_id) + s_list[int(c)] * (k_id - f)


class Worker(Process):
    def __init__(self, name, min_delay, max_delay, queue):
        super(Worker, self).__init__()
        self.exit_event = Event()
        log.debug('New Worker: %s' % name)
        self.name = name
        self.min_delay = min_delay
        self.max_delay = max_delay
        self.stat_queue = queue

    def turn_off(self):
        self.exit_event.set()

    def send_result(self, name, status, resp, exec_time):
        params = {
            'worker_name': self.name,
            'func_name': name,
            'status': status,
            'exec_time': exec_time,
            'response': resp,
        }
        if status != 'success':
            params['status_code'] = get_resp_field(resp, 'status_code')
        self.stat_queue.put_nowait(params)

    def sleep(self):
        sleep_time = float(self.min_delay) + random.random()*(
            float(self.max_delay) - float(self.min_delay))
        log.debug('Sleep for %.3f sec.', sleep_time)
        time.sleep(sleep_time)

    def scenario(self):
        self.sleep()
        card_data = generate_random_card_data(mode='full')
        status, resp, exec_time = wrapped_call_proxy('tokenize_card', card_data)
        self.send_result('tokenize_card', status, resp, exec_time)
        if status != 'success':
            log.debug('Error raised in tokenize_card')
            return
        card_token = get_resp_field(resp, 'card_token')
        cvn_token = get_resp_field(resp, 'cvn_token')
        if not card_token:
            log.debug('Cant find card token in tokenize_card response!')
            return
        if not cvn_token:
            log.debug('Cant find cvn token in tokenize_card response!')
            return
        status, resp, exec_time = wrapped_call_proxy('detokenize_card',
                                                     card_token, cvn_token)
        self.send_result('detokenize_card', status, resp, exec_time)
        if status != 'success':
            log.debug('Error raised in detokenize_card')
            return
        status, resp, exec_time = wrapped_call_proxy('remove_card',
                                                     card_token, cvn_token)
        self.send_result('remove_card', status, resp, exec_time)
        if status != 'success':
            log.debug('Error raised in remove_card')

    def run(self):
        try:
            while not self.exit_event.is_set():
                t0 = time.time()
                try:
                    self.scenario()
                except Exception, e:
                    log.debug('Error raised in scenario: %s', str(e))
                    self.send_result('scenario_failed', 'exception', repr(e),
                                     time.time() - t0)
        except KeyboardInterrupt:
            pass


class WorkController:
    def __init__(self, max_workers, min_delay=0.0, max_delay=0.0,
                 test_time=None, only_finish_log=False):
        self.workers = []
        self.method_stats = {}
        self.max_workers = max_workers
        self.min_delay = min_delay
        self.max_delay = max_delay
        self.stat_queue = Queue()
        self.start()
        self.refresh_rate = 1.0
        self.started_ts = time.time()
        self.test_time = test_time
        self.only_finish_log = only_finish_log

    def start(self):
        log.debug('Init working pool...')
        for i in range(self.max_workers):
            worker = Worker('Process #%i' % i, self.min_delay,
                            self.max_delay, self.stat_queue)
            # worker.daemon = True
            self.workers.append(worker)

    def handle_queue(self):
        try:
            thread_resp = self.stat_queue.get(True, 1.0)
            func_name = thread_resp.get('func_name')
            if func_name not in self.method_stats:
                self.method_stats[func_name] = ApiMethodStat(func_name)
            self.method_stats[func_name].add(thread_resp)
        except Empty:
            pass

    def print_stat(self):
        time_elapsed = time.time() - self.started_ts
        calls = [stat.total_calls for name, stat
                 in self.method_stats.iteritems()
                 if name != 'scenario_failed']
        rps = min(calls or [0]) / time_elapsed
        print 'Call stat: (%.3f sec elapsed)  ~%.1f rps' % (time_elapsed, rps)
        print 'Function name        | Total | MinTime | AvgTime | '\
              'MaxTime |   75%   |   90%   |   95%   |   99%   | Errors'
        format_str = '%(name)20s | %(total)5i | %(min)7.3f | %(avg)7.3f | '\
                     '%(max)7.3f | %(75p)7.3f | %(90p)7.3f | %(95p)7.3f | '\
                     '%(99p)7.3f | %(errors)s'
        for name, stat in self.method_stats.iteritems():
            print format_str % stat.get_stat()
        total_avg_delay = sum(stat.avg_exec_time for name, stat
                              in self.method_stats.iteritems()
                              if name != 'scenario_failed')
        print 'total avg delay = %.3f' % total_avg_delay

    def run(self):
        print 'Start load test with %i workers' % self.max_workers
        log.debug("Start working pool")
        p_time = time.time()
        p0_time = p_time
        for worker in self.workers:
            worker.start()
        while True:
            self.handle_queue()
            c_time = time.time()
            if c_time - p0_time >= self.test_time:
                break
            if not self.only_finish_log \
                    and c_time - p_time > self.refresh_rate:
                self.print_stat()
                p_time = c_time
        self.stop()

    def stop(self):
        if not self.workers:
            return
        log.debug("Stop working pool")
        for worker in self.workers:
            log.debug("Turn off: %s" % worker.name)
            worker.turn_off()
        for worker in self.workers:
            log.debug("Waiting: %s" % worker.name)
            worker.join()
        while not self.stat_queue.empty():
            self.handle_queue()
        if self.only_finish_log:
            self.print_stat()
        self.workers = []


def main():
    controller = None
    try:
        threads = [1, 3, 6, 12]
        for t_count in threads:
            controller = WorkController(t_count, 0.0, 0.02, 15.0, False)
            controller.run()
        controller = None
    except KeyboardInterrupt:
        log.debug('Got KeyBoardInterrupt signal...')
    finally:
        if controller:
            controller.stop()
            controller = None
    log.debug('Exit from runner...')


if __name__ == "__main__":
    main()

# vim:ts=4:sts=4:sw=4:tw=85:et:
