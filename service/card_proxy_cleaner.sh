#!/bin/sh

SITE_PACKAGES=$(python -c "import card_proxy_service as x; \
print '/'.join(x.__file__.split('/')[:-2])")
export CONFIG_FILE=/etc/card_proxy_service/card_proxy_cleaner.cfg.xml
python ${SITE_PACKAGES}/card_proxy_service/card_proxy_cleaner.py \
    -random-delay \
    >> /var/log/cpr_service/card_proxy_cleaner.log 2>&1
