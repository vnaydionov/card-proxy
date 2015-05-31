
add_executable (core_tests
    core_tests.cpp ../utils.cpp ../aes_crypter.cpp
    )

target_link_libraries (core_tests crypto ssl)

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CMAKE_SOURCE_DIR}/src
    )
