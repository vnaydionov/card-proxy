
# Python site-packages path
EXECUTE_PROCESS(COMMAND python -c "import MySQLdb as x; print chr(47).join(x.__file__.split(chr(47))[2:-2])"
                OUTPUT_VARIABLE SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)

install (FILES functions.sh mk_users.sh DESTINATION share/card_proxy_service)

install (PROGRAMS
         card_proxy_keyproc.sh
         card_proxy_hmacproc.sh
         card_proxy_cleaner.sh
         DESTINATION bin)

install (FILES
         __init__.py
         application.py
         card_proxy_keyproc.py
         card_proxy_hmacproc.py
         card_proxy_cleaner.py
         DESTINATION ${SITE_PACKAGES}/card_proxy_service)

install (FILES
         card_proxy_keyproc.cfg.xml
         card_proxy_hmacproc.cfg.xml
         card_proxy_cleaner.cfg.xml
         DESTINATION etc/card_proxy_service)

install (FILES card_proxy_service.cron
         DESTINATION etc/cron.d RENAME card-proxy-service)


install (PROGRAMS card_proxy_keyman.py DESTINATION bin RENAME card_proxy_keyman)
