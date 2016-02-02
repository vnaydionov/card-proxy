
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CURL_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/xxutils
    )

add_executable (key_keeper2
    key_keeper2.cpp
    key_keeper_logic.cpp
    )

target_link_libraries (key_keeper2
    xxutils
    crypto
    ssl
    ${CURL_LIBRARIES}
    ${YBUTIL_LIB}
    ${YBORM_LIB}
    ${YB_BOOST_LIBS}
    )

install (TARGETS key_keeper2 DESTINATION bin)
install (PROGRAMS key_keeper2-ping key_keeper2-restarter DESTINATION bin)
install (FILES key_keeper2.cfg.xml DESTINATION etc/card_proxy)
install (PROGRAMS key_keeper2.init DESTINATION etc/init.d RENAME key-keeper2)
install (FILES key_keeper2.logrotate DESTINATION etc/logrotate.d
         RENAME card_proxy_keykeeper2)
install (FILES key_keeper2.nginx DESTINATION etc/nginx/sites-available
         RENAME 10-card_proxy_keykeeper2)

