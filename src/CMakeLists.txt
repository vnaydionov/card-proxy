
add_subdirectory (domain)
add_subdirectory (tests)

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CPPREST_INCLUDES}
    ${CMAKE_CURRENT_BINARY_DIR})

add_library (cardkeeper STATIC
    utils.cpp aes_crypter.cpp card_crypter.cpp dek_pool.cpp
    conf_reader.cpp tcp_socket.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Card.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/IncomingRequest.cpp
    )

set (LOGIC_DEBUG_CPP)
if (USE_VAULT_DEBUG_API)
    set (LOGIC_DEBUG_CPP logic_debug.cpp)
    add_definitions (-DVAULT_DEBUG_API)
endif ()

add_executable (card_proxy_cb
    app_class.cpp servant_utils.cpp ${LOGIC_DEBUG_CPP} logic_token.cpp
    main_cb.cpp
    )

add_executable (card_proxy
    app_class.cpp servant_utils.cpp ${LOGIC_DEBUG_CPP} logic_token.cpp
    main_mh.cpp micro_http.cpp
    )

set_source_files_properties (
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Card.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/IncomingRequest.cpp
    PROPERTIES GENERATED 1
    )

add_dependencies (cardkeeper generate_domain)

target_link_libraries (
    card_proxy_cb cardkeeper crypto ssl
    ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS}
    ${CPPREST_LIB}
)

target_link_libraries (
    card_proxy cardkeeper crypto ssl
    ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS}
    ${CPPREST_LIB}
)

install (TARGETS card_proxy DESTINATION bin)
install (PROGRAMS card_proxy-ping card_proxy-restarter DESTINATION bin)
install (FILES card_proxy_schema.xml DESTINATION share/card_proxy)
install (FILES key_settings.cfg.xml card_proxy.cfg.xml DESTINATION etc/card_proxy)
install (PROGRAMS card-proxy.init DESTINATION etc/init.d RENAME card-proxy)
install (FILES card-proxy.logrotate DESTINATION etc/logrotate.d)

