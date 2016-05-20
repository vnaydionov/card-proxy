// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "proxy_any.h"
#include "app_class.h"
#include "servant_utils.h"
#include <util/string_utils.h>

const HttpHeaders convert_headers(const HttpRequest &req)
{
    HttpHeaders result;
    auto i = req.headers().begin(), iend = req.headers().end();
    for (; i != iend; ++i) {
        const auto h_lower = Yb::StrUtils::str_to_lower(i->first);
        if (h_lower == "host" || h_lower == "x-forwarded-for" || h_lower == "x-real-ip")
            continue;
        result[i->first] = i->second;
    }
    return result;
}

void dump_nested_response(const HttpResponse &nested_response,
                          Yb::ILogger &logger)
{
    Yb::ILogger::Ptr nest_logger(logger.new_logger("nested").release());
    nest_logger->debug(Yb::to_string(nested_response.resp_code())
            + " " + nested_response.resp_desc());
    auto i = nested_response.headers().begin(),
         iend = nested_response.headers().end();
    for (; i != iend; ++i) {
        nest_logger->debug(i->first + ": " + i->second);
    }
    nest_logger->debug("body: " + nested_response.body());
}

const HttpResponse convert_response(const HttpResponse &nested_response,
                                    Yb::ILogger &logger)
{
    dump_nested_response(nested_response, logger);
    HttpResponse response(HTTP_1_0, nested_response.resp_code(),
            nested_response.resp_desc());
    auto i = nested_response.headers().begin(),
         iend = nested_response.headers().end();
    for (; i != iend; ++i) {
        if (i->first != "Transfer-Encoding")
            response.set_header(i->first, i->second);
    }
    response.set_response_body(nested_response.body(), "");
    return response;
}

const HttpResponse proxy_any(Yb::ILogger &logger,
                             const HttpRequest &request,
                             const std::string &target_uri,
                             const std::string &client_cert,
                             const std::string &client_privkey,
                             BodyProcessor bproc,
                             ParamsProcessor pproc)
{
    logger.info("proxy pass to " + target_uri);

    bool ssl_validate = theApp::instance().is_prod();

    std::string target_uri_fixed = target_uri;
    std::string body_fixed = request.body();

    if (bproc != NULL) {
        body_fixed = bproc(logger, body_fixed);
    }
    else if (pproc != NULL) {
        Yb::StringDict new_params = pproc(logger, request.params());
        if (request.method() == "GET") {
            target_uri_fixed += "?" + serialize_params(new_params);
            body_fixed = "";
        }
        else {
            body_fixed = serialize_params(new_params);
        }
    }

    HttpHeaders nested_req_headers = convert_headers(request);
    HttpResponse resp = http_post(
        target_uri_fixed,
        &logger,
        30.0,
        request.method(),
        nested_req_headers,
        HttpParams(),
        body_fixed,
        ssl_validate,
        client_cert,
        client_privkey);
    return convert_response(resp, logger);
}

// vim:ts=4:sts=4:sw=4:et:
