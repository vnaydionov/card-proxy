#ifndef _CARD_PROXY__HELPERS_H_
#define _CARD_PROXY__HELPERS_H_

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
int run_server_app(const std::string &log_name, const std::string &db_name,
        int port, HttpHandler *handlers_array, int n_handlers,
        const std::string &error_content_type,
        const std::string &error_body,
        const std::string &prefix = "")
{
    randomize();
    Yb::ILogger::Ptr log;
    try {
        theApp::instance().init(log_name, db_name);
        log.reset(theApp::instance().new_logger("main").release());
    }
    catch (const std::exception &ex) {
        std::cerr << "exception: " << ex.what() << "\n";
        return 1;
    }
    try {
        typedef HttpServer<HttpHandler> MyHttpServer;
        typename MyHttpServer::HandlerMap handlers;
        for (int i = 0; i < n_handlers; ++i)
            handlers[prefix + handlers_array[i].name()] = handlers_array[i];
        MyHttpServer server(
                port, handlers, &theApp::instance(),
                error_content_type, error_body);
        server.serve();
    }
    catch (const std::exception &ex) {
        log->error(std::string("exception: ") + ex.what());
        return 1;
    }
    return 0;
}

#endif // _CARD_PROXY__HELPERS_H_
// vim:ts=4:sts=4:sw=4:et:
