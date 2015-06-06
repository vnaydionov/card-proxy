
add_subdirectory (domain)
add_subdirectory (tests)

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CMAKE_CURRENT_BINARY_DIR})

add_library (cardkeeper STATIC
    utils.cpp aes_crypter.cpp card_crypter.cpp dek_pool.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Card.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/IncomingRequest.cpp
    )

add_executable (card_proxy
    tcp_socket.cpp micro_http.cpp app_class.cpp helpers.cpp main.cpp
    )

set_source_files_properties (
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Card.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/IncomingRequest.cpp
    PROPERTIES GENERATED 1
    )

add_dependencies (cardkeeper generate_domain)

target_link_libraries (card_proxy cardkeeper crypto ssl ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS})

install (TARGETS card_proxy DESTINATION bin)
install (FILES card_proxy_schema.xml DESTINATION shared)

