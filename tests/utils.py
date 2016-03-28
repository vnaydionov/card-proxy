import os
import random
from baluhn import generate, verify


def generate_random_number(length):
    def byte_to_int(byte):
        return '%i' % (ord(byte) * 10 // 256)
    rand_seq = os.urandom(length)
    result = ''.join([byte_to_int(elem) for elem in rand_seq])
    return result


def generate_random_string(length):
    def byte_to_char(byte):
        val = ord(byte) * (26*2 + 10) // 256
        if val < 26:
            return chr(ord('A') + val)
        elif val < 26*2:
            return chr(ord('a') + (val - 26))
        else:
            return chr(ord('0') + (val - 26*2))
    rand_seq = os.urandom(length)
    result = ''.join([byte_to_char(elem) for elem in rand_seq])
    return result


def generate_pan(pan_len=16):
    prefix = '500000'  # not to mess with real world cards
    base = prefix + str(random.randint(
        10 ** (pan_len - len(prefix) - 2),
        10 ** (pan_len - len(prefix) - 1) - 1))
    pan = base + baluhn.generate(base)
    assert baluhn.verify(pan)
    #print "pan=%s" % pan
    return pan


class CardData(object):
    def __init__(self, **kwargs):
        for k, v in kwargs.iteritems():
            setattr(self, k, v)

    def to_dict(self):
        return dict([
            (k, getattr(self, k)) for k in dir(self)
            if not (k.startswith('_') or callable(getattr(self, k)))
            ])


def generate_random_card_data(mode='full', pan_len=16):
    if mode == 'full':
        card_data = CardData(
            pan=generate_pan(),
            expire_year=random.randrange(2016, 2030),
            expire_month=random.randrange(1, 13),
            card_holder=generate_random_string(20),
            cvn=generate_random_number(3)
        )
    elif mode == 'without_cvn':
        card_data = CardData(
            pan=generate_pan(),
            expire_year=random.randrange(2016, 2030),
            expire_month=random.randrange(1, 13),
            card_holder=generate_random_string(20),
        )
    elif mode == 'only_cvn':
        card_data = CardData(
            cvn=generate_random_number(3)
        )
    else:
        raise RuntimeError('Invalid mode [%s]' % mode)
    return card_data

# vim:ts=4:sts=4:sw=4:tw=85:et:
