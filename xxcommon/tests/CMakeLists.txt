
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${PROJECT_SOURCE_DIR}/xxutils
    ${PROJECT_SOURCE_DIR}/xxcommon
    ${PROJECT_BINARY_DIR}/xxcommon)

add_executable (core_tests core_tests.cpp)

target_link_libraries (core_tests xxcommon xxutils crypto ssl
                       ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS}
                       ${CURL_LIBRARIES} ${JSONC_LIB})

add_executable (test_trans test_trans.cpp)

target_link_libraries (test_trans ${YBORM_LIB} ${YBUTIL_LIB} ${YB_BOOST_LIBS})

