
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CURL_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/xxutils
    )

add_executable (card_proxy_confpatch
    confpatch.cpp
    confpatch_logic.cpp
    )

target_link_libraries (card_proxy_confpatch
    xxutils
    crypto
    ssl
    ${CURL_LIBRARIES}
    ${YBUTIL_LIB}
    ${YBORM_LIB}
    ${YB_BOOST_LIBS}
    )

install (TARGETS card_proxy_confpatch DESTINATION bin)
install (PROGRAMS
         card_proxy_confpatch-ping
         card_proxy_confpatch-restarter
         card_proxy_confpatch-helper
         DESTINATION bin)
install (FILES card_proxy_confpatch.cfg.xml
         DESTINATION etc/card_proxy_confpatch)
install (PROGRAMS card_proxy_confpatch.init
         DESTINATION etc/init.d RENAME card_proxy_confpatch)
install (FILES card_proxy_confpatch.logrotate
         DESTINATION etc/logrotate.d RENAME card_proxy_confpatch)
install (FILES card_proxy_confpatch.nginx DESTINATION etc/nginx/sites-available
         RENAME 10-card_proxy_confpatch)
install (FILES card_proxy_confpatch.sudo DESTINATION etc/sudoers.d
         RENAME card_proxy_confpatch)

