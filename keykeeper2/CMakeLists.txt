
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CURL_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/xxutils
    )

add_executable (card_proxy_keykeeper2
    key_keeper2.cpp
    key_keeper_logic.cpp
    )

target_link_libraries (card_proxy_keykeeper2
    xxutils
    crypto
    ssl
    ${CURL_LIBRARIES}
    ${YBUTIL_LIB}
    ${YBORM_LIB}
    ${YB_BOOST_LIBS}
    )

install (TARGETS card_proxy_keykeeper2 DESTINATION bin)
install (PROGRAMS card_proxy_keykeeper2-ping card_proxy_keykeeper2-restarter
         DESTINATION bin)
install (FILES card_proxy_keykeeper2.cfg.xml
         DESTINATION etc/card_proxy_keykeeper2)
install (PROGRAMS card_proxy_keykeeper2.init
         DESTINATION etc/init.d RENAME card_proxy_keykeeper2)
install (FILES card_proxy_keykeeper2.nginx DESTINATION etc/nginx/sites-available
         RENAME 10-card_proxy_keykeeper2)

