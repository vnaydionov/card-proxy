// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__HELPERS_H
#define CARD_PROXY__HELPERS_H

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <util/nlogger.h>
#include <util/element_tree.h>
#include <orm/data_object.h>
#include "micro_http.h"
#include "app_class.h"

const std::string money2str(const Yb::Decimal &x);
const std::string timestamp2str(double ts);
double datetime2timestamp(const Yb::DateTime &d);
double datetime_diff(const Yb::DateTime &a, const Yb::DateTime &b);
const std::string dict2str(const Yb::StringDict &params);
const Yb::StringDict json2dict(const std::string &json_str);
void randomize();

class TimerGuard
{
    Yb::ILogger &logger_;
    Yb::MilliSec t0_;
    bool except_;
public:
    TimerGuard(Yb::ILogger &logger)
        : logger_(logger), t0_(Yb::get_cur_time_millisec()), except_(true)
    {}
    void set_ok() { except_ = false; }
    Yb::MilliSec time_spent() const
    {
        return Yb::get_cur_time_millisec() - t0_;
    }
    ~TimerGuard();
};

class ApiResult: public std::runtime_error
{
    Yb::ElementTree::ElementPtr p_;
public:
    ApiResult(Yb::ElementTree::ElementPtr p)
        : runtime_error("api result"), p_(p)
    {}
    virtual ~ApiResult() throw () {}
    Yb::ElementTree::ElementPtr result() const { return p_; }
};

template <class HttpHandler>
int run_server_app(const std::string &config_name,
        HttpHandler *handlers_array, int n_handlers)
{
    randomize();
    Yb::ILogger::Ptr log;
    try {
        theApp::instance().init(IConfig::Ptr(new XmlConfig(config_name)));
        log.reset(theApp::instance().new_logger("main").release());
    }
    catch (const std::exception &ex) {
        std::cerr << "exception: " << ex.what() << "\n";
        return 1;
    }
    try {
        typedef HttpServer<HttpHandler> MyHttpServer;
        typename MyHttpServer::HandlerMap handlers;
        std::string bind_host = "0.0.0.0";
        int bind_port = theApp::instance().cfg().get_value_as_int("card_proxy/port");
        std::string prefix = theApp::instance().cfg().get_value("card_proxy/prefix");
        for (int i = 0; i < n_handlers; ++i)
            handlers[prefix + handlers_array[i].name()] = handlers_array[i];
        MyHttpServer server(
                bind_host, bind_port, 30, handlers, &theApp::instance(),
                "application/json", "{ \"error\": \"unknown_error\" }");
        server.serve();
    }
    catch (const std::exception &ex) {
        log->error(std::string("exception: ") + ex.what());
        return 1;
    }
    return 0;
}

#endif // CARD_PROXY__HELPERS_H
// vim:ts=4:sts=4:sw=4:et:
