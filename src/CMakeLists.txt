
add_subdirectory (domain)

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CMAKE_CURRENT_BINARY_DIR})

add_executable (card_proxy
    tcp_socket.cpp micro_http.cpp app_class.cpp helpers.cpp main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Card.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp)

set_source_files_properties (
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Card.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    PROPERTIES GENERATED 1)

add_dependencies (card_proxy generate_domain)

target_link_libraries (card_proxy ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS})

install (TARGETS card_proxy DESTINATION bin)
install (FILES card_proxy_schema.xml DESTINATION shared)

