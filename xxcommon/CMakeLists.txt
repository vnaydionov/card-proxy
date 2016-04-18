
add_subdirectory (domain)
add_subdirectory (tests)

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${PROJECT_SOURCE_DIR}/xxutils
    ${PROJECT_SOURCE_DIR}/xxcommon
    ${CMAKE_CURRENT_BINARY_DIR})

add_library (xxcommon STATIC
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataToken.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/VaultUser.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/SecureVault.cpp
    dek_pool.cpp
    tokenizer.cpp
    card_crypter.cpp)

set_source_files_properties (
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataToken.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/DataKey.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/Config.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/VaultUser.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/domain/SecureVault.cpp
    PROPERTIES GENERATED 1)

add_dependencies (xxcommon generate_domain)

install (FILES card_proxy_schema.xml DESTINATION share/card_proxy_tokenizer)

