
add_executable (card_proxy_tests
    main_test.cpp ../utils.cpp ../aes_crypter.cpp
    )

target_link_libraries (card_proxy_tests crypto ssl)

include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CMAKE_SOURCE_DIR}/src
    )
