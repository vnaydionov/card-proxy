
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CURL_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/xxutils
    ${PROJECT_SOURCE_DIR}/xxcommon
    ${PROJECT_BINARY_DIR}/xxcommon
    )

add_executable (card_proxy_keyapi
    keyapi.cpp
    keyapi_logic.cpp
    )

target_link_libraries (card_proxy_keyapi
    xxcommon
    xxutils
    crypto
    ssl
    ${CURL_LIBRARIES}
    ${YBUTIL_LIB}
    ${YBORM_LIB}
    ${YB_BOOST_LIBS}
    )

install (TARGETS card_proxy_keyapi DESTINATION bin)
install (PROGRAMS
         card_proxy_keyapi-ping
         card_proxy_keyapi-restarter
         DESTINATION bin)
install (FILES card_proxy_keyapi.cfg.xml
         DESTINATION etc/card_proxy_keyapi)
install (PROGRAMS card_proxy_keyapi.init
         DESTINATION etc/init.d RENAME card_proxy_keyapi)
install (FILES card_proxy_keyapi.logrotate
         DESTINATION etc/logrotate.d RENAME card_proxy_keyapi)
install (FILES card_proxy_keyapi.nginx DESTINATION etc/nginx/sites-available
         RENAME 10-card_proxy_keyapi)

