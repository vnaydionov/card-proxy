#!/bin/sh

SITE_PACKAGES=$(python -c "import card_proxy_service as x; \
print '/'.join(x.__file__.split('/')[:-2])")
export CONFIG_FILE=/etc/card_proxy_service/card_proxy_hmacproc.cfg.xml
python ${SITE_PACKAGES}/card_proxy_service/card_proxy_hmacproc.py \
    -random-delay -shuffle-batches \
    >> /var/log/cpr_service/card_proxy_hmacproc.log 2>&1
