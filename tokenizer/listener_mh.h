// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LISTENER_MH_H
#define CARD_PROXY__LISTENER_MH_H

#include <util/nlogger.h>
#include <util/element_tree.h>
#include <orm/data_object.h>

#include "app_class.h"
#include "servant_utils.h"
#include "http_post.h"
#include "micro_http.h"

inline
const HttpHeaders convert_headers(const HttpMessage &req)
{
    HttpHeaders result;
    auto i = req.get_headers().begin(), iend = req.get_headers().end();
    for (; i != iend; ++i)
        result[i->first] = i->second;
    return result;
}

inline
void dump_nested_response(const HttpResponse &nested_response,
                          Yb::ILogger &logger)
{
    Yb::ILogger::Ptr nest_logger(logger.new_logger("nested").release());
    nest_logger->debug(Yb::to_string(nested_response.get<0>()
            + " " + nested_response.get<1>()));
    auto i = nested_response.get<3>().begin(),
         iend = nested_response.get<3>().end();
    for (; i != iend; ++i) {
        nest_logger->debug(i->first + ": " + i->second);
    }
    nest_logger->debug("body: " + nested_response.get<2>());
}

inline
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

inline
const HttpMessage proxy_any(Yb::ILogger &logger,
                            const HttpMessage &request,
                            const std::string &target_uri,
                            const std::string &client_cert = "",
                            const std::string &client_privkey = "",
                            BodyProcessor bproc = NULL,
                            ParamsProcessor pproc = NULL)
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

    HttpResponse resp = http_post(
        target_uri_fixed,
        &logger,
        30.0,
        request.get_method(),
        convert_headers(request),
        HttpParams(),
        body_fixed,
        ssl_validate,
        client_cert,
        client_privkey);
    return convert_response(resp, logger);
}

typedef XmlHttpWrapper CardProxyHttpWrapper;

template <class HttpHandler>
inline int run_server_app(const std::string &config_name,
        HttpHandler *handlers_array, int n_handlers)
{
    randomize();
    Yb::ILogger::Ptr logger;
    try {
        theApp::instance().init(IConfig::Ptr(new XmlConfig(config_name)));
        logger.reset(theApp::instance().new_logger("main").release());
    }
    catch (const std::exception &ex) {
        std::cerr << "exception: " << ex.what() << "\n";
        return 1;
    }
    try {
        std::string bind_host = theApp::instance().cfg()
            .get_value("HttpListener/Host");
        int bind_port = theApp::instance().cfg()
            .get_value_as_int("HttpListener/Port");
        std::string listen_at = "http://" + bind_host + ":"
                + Yb::to_string(bind_port) + "/";
        logger->error("listen at: " + listen_at);
        typedef HttpServer<HttpHandler> MyHttpServer;
        typename MyHttpServer::HandlerMap handlers;
        for (int i = 0; i < n_handlers; ++i) {
            std::string prefix = handlers_array[i].prefix();
            handlers[prefix + handlers_array[i].name()] = handlers_array[i];
        }
        std::string error_content_type = "text/json";
        std::string error_body = "{\"status\": \"internal_error\"}";
        MyHttpServer server(
                bind_host, bind_port, 30, handlers, &theApp::instance(),
                error_content_type, error_body);
        server.serve();
    }
    catch (const std::exception &ex) {
        logger->error(std::string("exception: ") + ex.what());
        return 1;
    }
    return 0;
}

#endif // CARD_PROXY__LISTENER_MH_H
// vim:ts=4:sts=4:sw=4:et:
