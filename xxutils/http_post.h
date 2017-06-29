// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__HTTP_POST_H
#define CARD_PROXY__HTTP_POST_H

#include <string>
#include <map>
#include <stdexcept>
#include <util/nlogger.h>
#include "http_message.h"
#include "servant_utils.h"

#define HTTP_POST_NO_LOGGER ((Yb::ILogger *)-1)

class HttpClientError: public std::runtime_error
{
public:
    HttpClientError(const std::string &msg);
};


typedef Yb::StringDict HttpParams;
typedef Yb::StringDict HttpHeaders;

const HttpResponse mk_http_resp(int http_code, const std::string &http_desc,
                                HttpHeaders &headers,
                                std::string &body);


const HttpResponse http_post(const std::string &uri,
    Yb::ILogger *logger = NULL,
    double timeout = 0,
    const std::string &method = "POST",
    const HttpHeaders &headers = HttpHeaders(),
    const HttpParams &params = HttpParams(),
    const std::string &body = "",
    bool ssl_validate = true,
    const std::string &client_cer = "",
    const std::string &client_key = "",
    bool dump_headers = false,
    const FiltersMap &filters = FiltersMap());

#endif // CARD_PROXY__HTTP_POST_H
// vim:ts=4:sts=4:sw=4:et:
