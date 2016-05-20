// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__PROXY_ANY_H
#define CARD_PROXY__PROXY_ANY_H

#include <util/nlogger.h>

#include "http_post.h"
#include "micro_http.h"
#include "processors.h"

const HttpHeaders convert_headers(const HttpRequest &req);

void dump_nested_response(const HttpResponse &nested_response,
                          Yb::ILogger &logger);

const HttpResponse convert_response(const HttpResponse &nested_response,
                                    Yb::ILogger &logger);

const HttpResponse proxy_any(Yb::ILogger &logger,
                             const HttpRequest &request,
                             const std::string &target_uri,
                             const std::string &client_cert = "",
                             const std::string &client_privkey = "",
                             BodyProcessor bproc = NULL,
                             ParamsProcessor pproc = NULL);

#endif // CARD_PROXY__PROXY_ANY_H
// vim:ts=4:sts=4:sw=4:et:
