# -*- coding: utf-8 -*-

import logging


def config_logger(filename):
    log = logging.getLogger()
    #log.setLevel(logging.DEBUG)
    log.setLevel(logging.CRITICAL)
    fhandler = logging.FileHandler(filename)
    formatter = logging.Formatter(
        u'%(filename)s[LINE:%(lineno)d]# %(levelname)-8s '
        u'[%(asctime)s]  %(message)s')
    fhandler.setLevel(logging.DEBUG)
    fhandler.setFormatter(formatter)
    log.addHandler(fhandler)
    return log


def config_sublogger(log):
    pass


def get_logger(filename):
    return config_logger(filename)

# vim:ts=4:sts=4:sw=4:tw=85:et:
