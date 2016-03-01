// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "proxy_any.h"
#include "servant_utils.h"

const HttpHeaders convert_headers(const HttpMessage &req)
{
    HttpHeaders result;
    auto i = req.get_headers().begin(), iend = req.get_headers().end();
    for (; i != iend; ++i)
        result[i->first] = i->second;
    return result;
}

void dump_nested_response(const HttpResponse &nested_response,
                          Yb::ILogger &logger)
{
    Yb::ILogger::Ptr nest_logger(logger.new_logger("nested").release());
    nest_logger->debug(Yb::to_string(nested_response.get<0>())
            + " " + nested_response.get<1>());
    auto i = nested_response.get<3>().begin(),
         iend = nested_response.get<3>().end();
    for (; i != iend; ++i) {
        nest_logger->debug(i->first + ": " + i->second);
    }
    nest_logger->debug("body: " + nested_response.get<2>());
}

const HttpMessage convert_response(const HttpResponse &nested_response,
                                   Yb::ILogger &logger)
{
    HttpMessage response(10, nested_response.get<0>(),
            nested_response.get<1>());
    auto i = nested_response.get<3>().begin(),
         iend = nested_response.get<3>().end();
    for (; i != iend; ++i) {
        response.set_header(i->first, i->second);
    }
    dump_nested_response(nested_response, logger);
    response.set_response_body(nested_response.get<2>());
    return response;
}

const HttpMessage proxy_any(Yb::ILogger &logger,
                            const HttpMessage &request,
                            const std::string &target_uri,
                            const std::string &client_cert,
                            const std::string &client_privkey,
                            BodyProcessor bproc,
                            ParamsProcessor pproc)
{
    logger.info("proxy pass to " + target_uri);

    // TODO: turn on validation in production code!
    bool ssl_validate = false;

    std::string target_uri_fixed = target_uri;
    std::string body_fixed = request.get_body();

    if (bproc != NULL) {
        body_fixed = bproc(logger, body_fixed);
    }
    else if (pproc != NULL) {
        Yb::StringDict new_params = pproc(logger, request.get_params());
        if (request.get_method() == "GET") {
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
        request.get_method(),
        nested_req_headers,
        HttpParams(),
        body_fixed,
        ssl_validate,
        client_cert,
        client_privkey);
    return convert_response(resp, logger);
}

// vim:ts=4:sts=4:sw=4:et:
