#!/bin/bash

SITE_PACKAGES=$(python -c "import MySQLdb as x; print '/'.join(x.__file__.split('/')[:-2])")
export CONFIG_FILE=/etc/card_proxy_service/card_proxy_keyproc.cfg.xml
sleep $((RANDOM%180))
python ${SITE_PACKAGES}/card_proxy_service/card_proxy_keyproc.py >> /var/log/cpr_service/card_proxy_keyproc.log 2>&1
