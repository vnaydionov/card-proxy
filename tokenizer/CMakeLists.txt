
add_subdirectory (domain)
add_subdirectory (tests)

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CPPREST_INCLUDES}
    ${PROJECT_SOURCE_DIR}/xxutils
    ${CMAKE_CURRENT_BINARY_DIR}
    )

add_library (cardkeeper STATIC
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataToken.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    dek_pool.cpp
    tokenizer.cpp
    card_crypter.cpp
    )

set (LOGIC_DEBUG_CPP)
if (USE_VAULT_DEBUG_API)
    set (LOGIC_DEBUG_CPP logic_debug.cpp)
    add_definitions (-DVAULT_DEBUG_API)
endif ()

add_executable (card_proxy_tokenizer
    ${LOGIC_DEBUG_CPP} logic_token.cpp
    main_mh.cpp
    )

set_source_files_properties (
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataToken.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    PROPERTIES GENERATED 1
    )

add_dependencies (cardkeeper generate_domain)

target_link_libraries (
    card_proxy_tokenizer cardkeeper xxutils crypto ssl
    ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS} ${CURL_LIBRARIES}
    ${CPPREST_LIB}
)

install (TARGETS card_proxy_tokenizer DESTINATION bin)
install (PROGRAMS card_proxy_tokenizer-ping card_proxy_tokenizer-restarter
         DESTINATION bin)
install (FILES card_proxy_schema.xml DESTINATION share/card_proxy_tokenizer)
install (FILES key_settings.cfg.xml card_proxy_tokenizer.cfg.xml
         DESTINATION etc/card_proxy_tokenizer)
install (PROGRAMS card_proxy_tokenizer.init
         DESTINATION etc/init.d RENAME card_proxy_tokenizer)
install (FILES card_proxy_tokenizer.logrotate
         DESTINATION etc/logrotate.d RENAME card_proxy_tokenizer)
install (FILES card_proxy_tokenizer.nginx DESTINATION etc/nginx/sites-available
         RENAME 10-card_proxy_tokenizer)

