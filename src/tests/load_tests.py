from multiprocessing import Process, Queue, Event
import os
import re
import time
import urllib
import random
import string
import logging

SERVER_URL = 'http://localhost:9119/card_bind'

LOG_PATH = './logs/'


class CustomStreamHandler(logging.StreamHandler):
    def emit(self, record):
        msg = record.msg.decode('utf-8', errors='replace')
        logging.StreamHandler.emit(self, record)

def create_logger(logger_name):
    logger = logging.getLogger(logger_name)
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter(fmt=u'%(processName)s # %(levelname)s [%(asctime)s]  %(message)s', datefmt='%m/%d/%Y %I:%M:%S %p')
    
    stream_handler = CustomStreamHandler()
    stream_handler.setLevel(logging.DEBUG)
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    if not os.path.exists(LOG_PATH):
        os.makedirs(LOG_PATH)
    file_handler = logging.FileHandler(LOG_PATH + 'main.log', 'a', 'utf8', 0)
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)
    return logger
    

def generate_random_pan(length=20):
    return ''.join([str(random.randint(0,9)) for _ in range(length)])

def generate_random_cvn(length=20):
    return random.randint(100, 999);

def generate_random_card_holder(length=20):
    return ''.join([str(random.choice(string.letters)) for _ in range(length)])

def timer(f):
    def time_f(*args):
        before = time.time()
        resp = f(*args)
        after = time.time()
        return after - before, resp
    return time_f

@timer
def get_token():
    params = {
        'pan': generate_random_pan(),
        'expire_year': random.randint(2015, 2020),
        'expire_month': random.randint(0, 11),
        'card_holder': generate_random_card_holder(),
        'cvn': generate_random_cvn(),
    }
    url_params = urllib.urlencode(params)
    resp = urllib.urlopen(SERVER_URL + '/get_token', url_params)
    return resp.read()

@timer
def get_card():
    params = {
        'token': '',
    }
    url_params = urllib.urlencode(params)
    resp = urllib.urlopen(SERVER_URL + '/get_card', url_params)
    

class Worker(Process):
    def __init__(self, name, min_delay, max_delay, queue):
        Process.__init__(self)
        self.exit = Event()
        self.logger = create_logger(name)
        self.logger.debug('New Worker: %s' % name)
        self.name = name
        self.min_delay = min_delay
        self.max_delay = max_delay
        self.queue = queue
        self.call_stat = {"get_token": {},
                          "get_card": {}}

    def turn_off(self):
        self.exit.set()

    def run(self):
        while not self.exit.is_set():
            sleep_time = random.randint(self.min_delay, self.max_delay) 
            self.logger.debug('sleep for %s' % str(sleep_time))
            time.sleep(sleep_time)
            #if random.randint(0, 1) == 0:
            exec_time, resp = get_token()
            result = ''.join(re.search("<status>(\w*)<\/status>", resp).groups()[:1])
            self.call_stat["get_token"]["calls"] = self.call_stat["get_token"].get("calls", 0) + 1
            self.call_stat["get_token"]["time"] = self.call_stat["get_token"].get("time", 0.0) + exec_time
            self.logger.debug('get_token() time: %s, result: %s' % (str(exec_time), result))
            #else:
            #    self.logger.debug('time for get_card(): %s' % str(get_card()))
        self.queue.put(self.call_stat, block=False)

class WorkingPool:
    def __init__(self, max_workers):
        self.workers = []
        self.max_workers = max_workers
        self.queue = Queue()
        self.start()

    def start(self):
        log.debug("Start working pool")
        for i in range(self.max_workers):
            worker = Worker('Process #%i' % i, 1, 10, self.queue)
            #worker.daemon = True
            self.workers.append(worker)

    def run(self):
        log.debug("Run working pool")
        for worker in self.workers:
            worker.start()

    def stop(self):
        log.debug("Stop working pool")
        for worker in self.workers:
            log.debug("Turn off: %s" % worker.name)
            worker.turn_off() 
        for worker in self.workers:
            log.debug("Waiting: %s" % worker.name)
            worker.join()
    
    def get_result(self):
        log.debug("Collect results")
        result = {'get_token': {'calls': 0, 'time': 0.0},
                  'get_card': {'calls': 0, 'time': 0.0}}
        while not self.queue.empty():
            call_stat = self.queue.get()
            result['get_token']['calls'] += call_stat['get_token']['calls']
            result['get_token']['time'] += call_stat['get_token']['time']

        result['get_token']['avg'] = result['get_token']['time'] / result['get_token']['calls']
        return result

log = create_logger("Main")

if __name__ == "__main__": 
    pool = WorkingPool(10)
    pool.run()
    time.sleep(200)
    pool.stop()
    print pool.get_result()
    

