
include_directories (
    ${BOOST_INCLUDEDIR}
    ${YBORM_INCLUDES}
    ${CURL_INCLUDE_DIR}
    )

add_library (xxutils STATIC
    aes_crypter.cpp
    app_class.cpp
    conf_reader.cpp
    http_message.cpp
    http_post.cpp
    micro_http.cpp
    servant_utils.cpp
    tcp_socket.cpp
    utils.cpp
    )

