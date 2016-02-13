// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__HTTP_POST_H
#define CARD_PROXY__HTTP_POST_H

#include <string>
#include <map>
#include <stdexcept>
#include <util/nlogger.h>
#include <boost/tuple/tuple.hpp>

typedef std::map<std::string, std::string> HttpParams;
typedef std::map<std::string, std::string> HttpHeaders;
typedef boost::tuple<int, std::string, std::string, HttpHeaders> HttpResponse;

class HttpClientError: public std::runtime_error
{
public:
    HttpClientError(const std::string &msg);
};

const HttpResponse http_post(const std::string &uri,
    Yb::ILogger *logger = NULL,
    double timeout = 0,
    const std::string &method = "POST",
    const HttpHeaders &headers = HttpHeaders(),
    const HttpParams &params = HttpParams(),
    const std::string &body = "",
    bool ssl_validate = true,
    const std::string &client_cer = "",
    const std::string &client_key = "");

#endif // CARD_PROXY__HTTP_POST_H
// vim:ts=4:sts=4:sw=4:et:
