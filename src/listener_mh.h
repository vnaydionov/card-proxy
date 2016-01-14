// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LISTENER_MH_H
#define CARD_PROXY__LISTENER_MH_H

#include <util/nlogger.h>
#include <util/element_tree.h>
#include <orm/data_object.h>

#include "app_class.h"
#include "servant_utils.h"
#include "micro_http.h"

inline
const std::string extract_body(web::http::http_response &response,
                               Yb::ILogger &logger)
{
    auto body_task = response.extract_string();
    if (body_task.wait() != pplx::completed)
        throw ::RunTimeError("pplx: body_task.wait() failed");
    return body_task.get();
}

inline
const web::http::http_headers convert_headers(const HttpHeaders &req)
{
    web::http::http_headers result;
    for (const auto &p : req.get_headers()) {
        result[p.first] = p.second;
    }
    return result;
}

inline
void dump_nested_response(web::http::http_response &nested_response,
                          const std::string &body,
                          Yb::ILogger &logger)
{
    Yb::ILogger::Ptr nest_logger(logger.new_logger("nested").release());
    nest_logger->debug(std::to_string(nested_response.status_code()) +
                        " " + nested_response.reason_phrase());
    for (const auto &p : nested_response.headers()) {
        nest_logger->debug(p.first + ": " + p.second);
    }
    nest_logger->debug("body: " + body);
}

inline
const HttpHeaders convert_response(pplx::task<web::http::http_response> &task,
                                   Yb::ILogger &logger)
{
    if (task.wait() != pplx::completed)
        throw ::RunTimeError("pplx: task.wait() failed");
    web::http::http_response nested_response = task.get();
    HttpHeaders response(10, nested_response.status_code(),
            nested_response.reason_phrase());
    for (const auto &p : nested_response.headers()) {
        response.set_header(p.first, p.second);
    }
    std::string body = extract_body(nested_response, logger);
    dump_nested_response(nested_response, body, logger);
    response.set_response_body(body);
    return response;
}

inline
const HttpHeaders proxy_any(Yb::ILogger &logger,
                            const HttpHeaders &request,
                            const std::string &target_uri,
                            const std::string &client_cert = "",
                            const std::string &client_privkey = "",
                            BodyProcessor bproc = NULL,
                            ParamsProcessor pproc = NULL)
{
    logger.info("proxy pass to " + target_uri);

    web::http::client::http_client_config client_config;
    if (!client_cert.empty())
        client_config.set_client_certificate(client_cert);
    if (!client_privkey.empty())
        client_config.set_client_private_key(client_privkey);

    // TODO: turn on validation in production code!
    client_config.set_validate_certificates(false);

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

    web::http::client::http_client client(target_uri_fixed, client_config);
    web::http::http_request nested_request(
            request.get_method() == "GET"?
            web::http::methods::GET: web::http::methods::POST);
    nested_request.headers() = convert_headers(request);
    nested_request.set_body(body_fixed);
    auto task = client.request(nested_request);
    return convert_response(task, logger);
}


typedef const HttpHeaders (*PlainHttpHandler)(
        Yb::ILogger &logger, const HttpHeaders &request);

typedef Yb::ElementTree::ElementPtr (*XmlHttpHandler)(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params);

class CardProxyHttpWrapper
{
    std::string name_, prefix_, default_status_;
    XmlHttpHandler f_;
    PlainHttpHandler g_;

    std::string dump_result(Yb::ILogger &logger, Yb::ElementTree::ElementPtr res)
    {
        std::string res_str = res->serialize();
        logger.info(name_ + ": Response: " + res_str);
        return res_str;
    }

    const HttpHeaders try_call(Yb::ILogger *logger, TimerGuard &t,
                               const HttpHeaders &request, int n)
    {
        if (f_)
        {
            const Yb::StringDict &params = request.get_params();
            logger->info("Method " + name_ + std::string(n? " re": " ") +
                    "started. Params: " + dict2str(params));
            // call xml wrapped
            std::auto_ptr<Yb::Session> session(
                    theApp::instance().new_session());
            Yb::ElementTree::ElementPtr res = f_(*session, *logger, params);
            session->commit();
            // form the reply
            HttpHeaders response(10, 200, _T("OK"));
            response.set_response_body(dump_result(*logger, res), _T("text/xml"));
            t.set_ok();
            return response;
        }
        else
        {
            logger->info("Method " + name_ + std::string(n? " re": " ") +
                    "started.");
            HttpHeaders response = g_(*logger, request);
            t.set_ok();
            return response;
        }
    }

public:
    CardProxyHttpWrapper(): f_(NULL) {}

    CardProxyHttpWrapper(const std::string &name, XmlHttpHandler f,
            const std::string &prefix = "",
            const std::string &default_status = "not_available")
        : name_(name), prefix_(prefix)
        , default_status_(default_status), f_(f), g_(NULL)
    {}

    CardProxyHttpWrapper(const std::string &name, PlainHttpHandler g,
            const std::string &prefix = "",
            const std::string &default_status = "not_available")
        : name_(name), prefix_(prefix)
        , default_status_(default_status), f_(NULL), g_(g)
    {}

    const std::string &name() const { return name_; }
    const std::string &prefix() const { return prefix_; }

    const HttpHeaders operator() (const HttpHeaders &request)
    {
        Yb::ILogger::Ptr logger(theApp::instance().new_logger("invoker"));
        TimerGuard t(*logger, name_);
        try {
            try {
                return try_call(logger.get(), t, request, 0);
            }
            catch (const Yb::DBError &ex) {
                logger->error(std::string("db exception: ") + ex.what());
                return try_call(logger.get(), t, request, 1);
            }
        }
        catch (const ApiResult &ex) {
            t.set_ok();
            HttpHeaders response(10, 200, _T("OK"));
            response.set_response_body(dump_result(*logger, ex.result()), _T("text/xml"));
            return response;
        }
        catch (const std::exception &ex) {
            logger->error(std::string("exception: ") + ex.what());
            HttpHeaders response(10, 500, _T("Internal error"));
            response.set_response_body(dump_result(*logger, mk_resp(default_status_)), _T("text/xml"));
            return response;
        }
        catch (...) {
            logger->error("unknown exception");
            HttpHeaders response(10, 500, _T("Internal error"));
            response.set_response_body(dump_result(*logger, mk_resp(default_status_)), _T("text/xml"));
            return response;
        }
    }
};

#define WRAP(func, prefix) CardProxyHttpWrapper(#func, func, prefix)

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
                + std::to_string(bind_port) + "/";
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
