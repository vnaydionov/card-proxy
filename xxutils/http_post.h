// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__HTTP_POST_H
#define CARD_PROXY__HTTP_POST_H

#include <string>
#include <map>
#include <stdexcept>
#include <util/nlogger.h>

typedef std::map<std::string, std::string> HttpParams;

class HttpClientError: public std::runtime_error
{
public:
    HttpClientError(const std::string &msg);
};

const std::string http_post(const std::string &uri,
    const HttpParams &params,
    double timeout = 0,
    const std::string &method = "POST",
    Yb::ILogger *logger = NULL);

#endif // CARD_PROXY__HTTP_POST_H
// vim:ts=4:sts=4:sw=4:et:
