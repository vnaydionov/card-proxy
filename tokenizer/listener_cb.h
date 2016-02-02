// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LISTENER_CB_H
#define CARD_PROXY__LISTENER_CB_H

#include <util/nlogger.h>
#include <util/element_tree.h>
#include <orm/data_object.h>
#include <cpprest/http_listener.h>

#include "app_class.h"
#include "servant_utils.h"

inline
const std::string extract_body(web::http::http_request &request,
                               Yb::ILogger &logger)
{
    auto body_task = request.extract_string();
    if (body_task.wait() != pplx::completed)
        throw ::RunTimeError("pplx: body_task.wait() failed");
    return body_task.get();
}

inline
const Yb::StringDict get_params(web::http::http_request &request,
                                Yb::ILogger &logger)
{
    // parse query string
    const auto &uri = request.relative_uri();
    auto uri_params = web::http::uri::split_query(uri.query());
    // get request body
    auto body = extract_body(request, logger);
    // check Content-Type
    std::map<std::string, std::string> body_params;
    const std::string prefix = "application/x-www-form-urlencoded";
    if (request.headers().content_type().substr(0, prefix.size())
            == prefix)
        body_params = web::http::uri::split_query(body);
    // fill params
    Yb::StringDict params;
    for (const auto &p: uri_params)
        params[p.first] = uri_decode(p.second);
    for (const auto &p: body_params)
        params[p.first] = uri_decode(p.second);
    return params;
}

inline
web::http::http_response proxy_any(Yb::ILogger &logger,
                                   web::http::http_request &request,
                                   const std::string &target_uri,
                                   const std::string &client_cert = "",
                                   const std::string &client_privkey = "",
                                   BodyProcessor bproc = NULL,
                                   ParamsProcessor pproc = NULL)
{
    web::http::client::http_client_config client_config;
    if (!client_cert.empty())
        client_config.set_client_certificate(client_cert);
    if (!client_privkey.empty())
        client_config.set_client_private_key(client_privkey);

    // TODO: turn on validation in production code!
    client_config.set_validate_certificates(false);

    std::string target_uri_fixed = target_uri;
    std::string body_fixed = extract_body(request, logger);

    if (bproc != NULL) {
        body_fixed = bproc(logger, body_fixed);
    }
    else if (pproc != NULL) {
        Yb::StringDict new_params = pproc(logger,
                                          get_params(request, logger));
        if (request.method() == web::http::methods::GET) {
            target_uri_fixed += "?" + serialize_params(new_params);
            body_fixed = "";
        }
        else {
            body_fixed = serialize_params(new_params);
        }
    }

    web::http::client::http_client client(target_uri_fixed, client_config);
    web::http::http_request nested_request(request.method());
    nested_request.headers() = request.headers();
    nested_request.set_body(body_fixed);
    auto task = client.request(nested_request);
    if (task.wait() != pplx::completed)
        throw ::RunTimeError("pplx: task.wait() failed");
    return task.get();
}


typedef web::http::http_response (*PlainCBHttpHandler)(
        Yb::ILogger &logger, web::http::http_request &request);

typedef Yb::ElementTree::ElementPtr (*XmlHttpHandler)(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params);

class CardProxyHttpWrapper
{
    std::string name_, prefix_, default_status_;
    XmlHttpHandler f_;
    PlainCBHttpHandler g_;

    std::string dump_result(Yb::ILogger &logger, Yb::ElementTree::ElementPtr res)
    {
        std::string res_str = res->serialize();
        logger.info(name_ + ": Response: " + res_str);
        return res_str;
    }

    void try_call(Yb::ILogger *logger, TimerGuard &t,
                  web::http::http_request &request, int n)
    {
        if (f_)
        {
            Yb::StringDict params = get_params(request, *logger);
            logger->info("Method " + name_ + std::string(n? " re": " ") +
                    "started. Params: " + dict2str(params));
            // call xml wrapped
            std::unique_ptr<Yb::Session> session(
                    theApp::instance().new_session());
            Yb::ElementTree::ElementPtr res = f_(*session, *logger, params);
            session->commit();
            // form the reply
            web::http::http_response resp(web::http::status_codes::OK);
            resp.set_body(dump_result(*logger, res), "text/xml");
            request.reply(resp);
        }
        else
        {
            logger->info("Method " + name_ + std::string(n? " re": " ") +
                    "started.");
            web::http::http_response resp = g_(*logger, request);
            request.reply(resp);
        }
        t.set_ok();
    }

public:
    CardProxyHttpWrapper(): f_(NULL) {}

    CardProxyHttpWrapper(const std::string &name, XmlHttpHandler f,
            const std::string &prefix = "",
            const std::string &default_status = "not_available")
        : name_(name), prefix_(prefix)
        , default_status_(default_status), f_(f), g_(NULL)
    {}

    CardProxyHttpWrapper(const std::string &name, PlainCBHttpHandler g,
            const std::string &prefix = "",
            const std::string &default_status = "not_available")
        : name_(name), prefix_(prefix)
        , default_status_(default_status), f_(NULL), g_(g)
    {}

    const std::string &name() const { return name_; }
    const std::string &prefix() const { return prefix_; }

    void operator() (web::http::http_request &request)
    {
        Yb::ILogger::Ptr logger(theApp::instance().new_logger("invoker"));
        TimerGuard t(*logger, name_);
        try {
            try {
                try_call(logger.get(), t, request, 0);
            }
            catch (const Yb::DBError &ex) {
                logger->error(std::string("db exception: ") + ex.what());
                try_call(logger.get(), t, request, 1);
            }
        }
        catch (const ApiResult &ex) {
            web::http::http_response resp(web::http::status_codes::OK);
            resp.set_body(dump_result(*logger, ex.result()), "text/xml");
            request.reply(resp);
            t.set_ok();
        }
        catch (const std::exception &ex) {
            logger->error(std::string("exception: ") + ex.what());
            web::http::http_response resp(web::http::status_codes::InternalError);
            resp.set_body(dump_result(*logger, mk_resp(default_status_)),
                    "text/xml");
            request.reply(resp);
        }
        catch (...) {
            logger->error("unknown exception");
            web::http::http_response resp(web::http::status_codes::InternalError);
            resp.set_body(dump_result(*logger, mk_resp(default_status_)),
                    "text/xml");
            request.reply(resp);
        }
    }
};

#define CWRAP(prefix, func) CardProxyHttpWrapper(#func, func, prefix)

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
        using namespace web::http;
        using namespace web::http::experimental::listener;
        http_listener listener(listen_at);
        auto http_handler = 
            [&](http_request request)
            {
                bool found = false;
                const auto &uri = request.relative_uri();
                for (int i = 0; i < n_handlers; ++i) {
                    std::string prefix = handlers_array[i].prefix();
                    if (uri.path() == prefix + handlers_array[i].name())
                    {
                        handlers_array[i](request);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    web::http::http_response resp(
                            web::http::status_codes::NotFound);
                    request.reply(resp);
                }
            };
        listener.support(methods::GET, http_handler);
        listener.support(methods::POST, http_handler);
        listener.open().wait();
        logger->error(std::string("open().wait() finished"));
        while (true) {
            sleep(100);
        }
    }
    catch (const std::exception &ex) {
        logger->error(std::string("exception: ") + ex.what());
        return 1;
    }
    return 0;
}

#endif // CARD_PROXY__LISTENER_CB_H
// vim:ts=4:sts=4:sw=4:et:
