
add_executable (core_tests
    core_tests.cpp
    )

target_link_libraries (core_tests cardkeeper crypto ssl ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS})

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CMAKE_SOURCE_DIR}/src
    )
