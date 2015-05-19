#include "helpers.h"
#include "domain/Card.h"
#include "domain/IncomingRequest.h"
#include "domain/DataKey.h"
#include "domain/Config.h"

using namespace std;
using namespace Yb;

ElementTree::ElementPtr mk_resp(const string &status,
        const string &status_code = "")
{
    ElementTree::ElementPtr res = ElementTree::new_element("response");
    res->sub_element("status", status);
    if (!status_code.empty())
        res->sub_element("status_code", status_code);
    char buf[40];
    MilliSec t = get_cur_time_millisec();
    sprintf(buf, "%u.%03u", (unsigned)(t/1000), (unsigned)(t%1000));
    res->sub_element("ts", buf);
    return res;
}

ElementTree::ElementPtr bind_card(Session &session, ILogger &logger,
        const StringDict &params)
{
    //
    ElementTree::ElementPtr resp = mk_resp("success");
    return resp;
}

typedef ElementTree::ElementPtr (*HttpHandler)(
        Session &session, ILogger &logger,
        const StringDict &params);

class CardProxyHttpWrapper
{
    string name_, default_status_;
    HttpHandler f_;
    string dump_result(ILogger &logger, ElementTree::ElementPtr res)
    {
        string res_str = res->serialize();
        logger.info("result: " + res_str);
        return res_str;
    }
public:
    CardProxyHttpWrapper(): f_(NULL) {}
    CardProxyHttpWrapper(const string &name, HttpHandler f,
            const string &default_status = "not_available")
        : name_(name), default_status_(default_status), f_(f)
    {}
    const string &name() const { return name_; }
    string operator() (const StringDict &params)
    {
        ILogger::Ptr logger(theApp::instance().new_logger(name_));
        TimerGuard t(*logger);
        try {
            logger->info("started, params: " + dict2str(params));
            int version = params.get_as<int>("version");
            YB_ASSERT(version >= 2);
            auto_ptr<Session> session(
                    theApp::instance().new_session());
            ElementTree::ElementPtr res = f_(*session, *logger, params);
            session->commit();
            t.set_ok();
            return dump_result(*logger, res);
        }
        catch (const ApiResult &ex) {
            t.set_ok();
            return dump_result(*logger, ex.result());
        }
        catch (const exception &ex) {
            logger->error(string("exception: ") + ex.what());
            return dump_result(*logger, mk_resp(default_status_));
        }
    }
};

#define WRAP(func) CardProxyHttpWrapper(#func, func)

int main(int argc, char *argv[])
{
    string log_name = "card_proxy.log";
    string db_name = "card_proxy_db";
    string error_content_type = "text/xml";
    string error_body = mk_resp("internal_error")->serialize();
    string prefix = "/card_bind/";
    int port = 9119;
    CardProxyHttpWrapper handlers[] = {
        WRAP(bind_card),
    };
    int n_handlers = sizeof(handlers)/sizeof(handlers[0]);
    return run_server_app(log_name, db_name, port,
            handlers, n_handlers, error_content_type, error_body, prefix);
}

// vim:ts=4:sts=4:sw=4:et:
