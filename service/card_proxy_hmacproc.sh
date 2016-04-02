#!/bin/sh

SITE_PACKAGES=$(python -c "import card_proxy_service as x; \
print '/'.join(x.__file__.split('/')[:-2])")
export CONFIG_FILE=/etc/card_proxy_service/card_proxy_hmacproc.cfg.xml

LOG_FILE=$(
    python -c "import xml.etree.ElementTree as et; \
               print et.parse('$CONFIG_FILE')\
                   .findtext('Log')" 2> /dev/null
)
if [ -z "$LOG_FILE" ] || [ "$LOG_FILE" = "syslog" ] ; then
    LOG_FILE="/dev/null"
fi

python ${SITE_PACKAGES}/card_proxy_service/card_proxy_hmacproc.py \
    -random-delay -shuffle-batches \
    >> $LOG_FILE 2>&1
