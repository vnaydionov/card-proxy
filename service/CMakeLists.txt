
# Python site-packages path
EXECUTE_PROCESS(COMMAND python -c "import MySQLdb as x; print chr(47).join(x.__file__.split(chr(47))[2:-2])"
                OUTPUT_VARIABLE SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)

install (FILES functions.sh DESTINATION share/card_proxy_service)

install (PROGRAMS card_proxy_cleaner.sh DESTINATION bin)

install (FILES card_proxy_cleaner.py __init__.py
         DESTINATION ${SITE_PACKAGES}/card_proxy_service)

install (FILES card_proxy_cleaner.cfg.xml
         DESTINATION etc/card_proxy_service)

install (FILES card_proxy_service.cron
         DESTINATION etc/cron.d RENAME card-proxy-service)

