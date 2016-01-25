
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${PROJECT_SOURCE_DIR}/xxutils
    ${PROJECT_SOURCE_DIR}/src
    )

add_executable (core_tests
    core_tests.cpp
    )

target_link_libraries (core_tests cardkeeper xxutils crypto ssl
                       ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS})

add_executable (test_trans test_trans.cpp)

target_link_libraries (test_trans ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS})

