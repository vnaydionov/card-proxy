# -*- coding: utf-8 -*-

import logging


def config_logger():
    log = logging.getLogger()
    log.setLevel(logging.DEBUG)
    fhandler = logging.FileHandler('proxy_tests.log')
    formatter = logging.Formatter(
        u'%(filename)s[LINE:%(lineno)d]# %(levelname)-8s '
        u'[%(asctime)s]  %(message)s')
    fhandler.setLevel(logging.DEBUG)
    fhandler.setFormatter(formatter)
    log.addHandler(fhandler)
    return log


def config_sublogger(log):
    pass


def get_logger():
    return config_logger()

# vim:ts=4:sts=4:sw=4:tw=85:et:
