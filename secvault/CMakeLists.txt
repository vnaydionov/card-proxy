
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${PROJECT_SOURCE_DIR}/xxutils
    ${PROJECT_SOURCE_DIR}/xxcommon
    ${CMAKE_CURRENT_BINARY_DIR}/../xxcommon)

add_executable (card_proxy_secvault
    logic_secvault.cpp
    logic_service.cpp servant.cpp)

target_link_libraries (
    card_proxy_secvault xxcommon xxutils crypto ssl
    ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS} ${CURL_LIBRARIES} ${JSONC_LIB})

install (TARGETS card_proxy_secvault DESTINATION bin)

install (PROGRAMS card_proxy_secvault-ping card_proxy_secvault-restarter
         DESTINATION bin)

install (FILES card_proxy_secvault.cfg.xml
         DESTINATION etc/card_proxy_secvault)

install (PROGRAMS card_proxy_secvault.init
         DESTINATION etc/init.d RENAME card_proxy_secvault)

install (FILES card_proxy_secvault.nginx DESTINATION etc/nginx/sites-available
         RENAME 10-card_proxy_secvault)

