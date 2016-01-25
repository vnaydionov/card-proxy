#ifndef _AUTH__XML_METHOD_H_
#define _AUTH__XML_METHOD_H_

#include <util/string_utils.h>
#include <util/element_tree.h>
#include "micro_http.h"
#include "servant_utils.h"

typedef const HttpHeaders (*PlainHttpHandler)(
        Yb::ILogger &logger, const HttpHeaders &request);

typedef Yb::ElementTree::ElementPtr (*XmlHttpHandler)(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params);

class XmlHttpWrapper
{
    Yb::String name_, prefix_, default_status_;
    XmlHttpHandler f_;
    PlainHttpHandler g_;
    boost::shared_ptr<Yb::ILogger> logger_;

    std::string dump_result(Yb::ElementTree::ElementPtr res)
    {
        std::string res_str = res->serialize();
        logger_->debug(name_ + ": Response: " + res_str);
        return res_str;
    }

    const HttpHeaders try_call(TimerGuard &t,
                               const HttpHeaders &request, int n)
    {
        logger_->info("Method " + NARROW(name_) +
                      std::string(n? " re": " ") + "started.");
        if (f_)
        {
            const Yb::StringDict &params = request.get_params();
            logger_->debug("Params: " + NARROW(dict2str(params)));
            // call xml wrapped
            std::auto_ptr<Yb::Session> session;
            if (theApp::instance().uses_db())
                session.reset(theApp::instance().new_session().release());
            Yb::ElementTree::ElementPtr res = f_(*session, *logger_, params);
            if (theApp::instance().uses_db())
                session->commit();
            // form the reply
            HttpHeaders response(10, 200, _T("OK"));
            response.set_response_body(dump_result(res), _T("application/xml"));
            t.set_ok();
            return response;
        }
        else
        {
            HttpHeaders response = g_(*logger_, request);
            t.set_ok();
            return response;
        }
    }

public:
    XmlHttpWrapper(): f_(NULL), g_(NULL) {}

    XmlHttpWrapper(const Yb::String &name, XmlHttpHandler f,
            const Yb::String &prefix = _T(""),
            const Yb::String &default_status = _T("internal_error"))
        : name_(name), prefix_(prefix)
        , default_status_(default_status), f_(f), g_(NULL)
        , logger_(theApp::instance().new_logger(
                    _T("invoker.xml")).release())
    {}

    XmlHttpWrapper(const Yb::String &name, PlainHttpHandler g,
            const Yb::String &prefix = _T(""),
            const Yb::String &default_status = _T("internal_error"))
        : name_(name), prefix_(prefix)
        , default_status_(default_status), f_(NULL), g_(g)
        , logger_(theApp::instance().new_logger(
                    _T("invoker.plain")).release())
    {}

    const Yb::String &name() const { return name_; }
    const Yb::String &prefix() const { return prefix_; }

    const HttpHeaders operator() (const HttpHeaders &request)
    {
        const Yb::String cont_type = _T("application/xml");
        TimerGuard t(*logger_, name_);
        try {
            try {
                return try_call(t, request, 0);
            }
            catch (const Yb::DBError &ex) {
                logger_->error(std::string("db exception: ") + ex.what());
                return try_call(t, request, 1);
            }
        }
        catch (const ApiResult &ex) {
            t.set_ok();
            HttpHeaders response(10, 200, _T("OK"));
            response.set_response_body(dump_result(ex.result()), cont_type);
            return response;
        }
        catch (const std::exception &ex) {
            logger_->error(std::string("exception: ") + ex.what());
            HttpHeaders response(10, 500, _T("Internal error"));
            response.set_response_body(dump_result(mk_resp(default_status_)), cont_type);
            return response;
        }
        catch (...) {
            logger_->error("unknown exception");
            HttpHeaders response(10, 500, _T("Internal error"));
            response.set_response_body(dump_result(mk_resp(default_status_)), cont_type);
            return response;
        }
    }
};

#define WRAP(prefix, func) XmlHttpWrapper(_T(#func), func, prefix)

#endif // _AUTH__XML_METHOD_H_
// vim:ts=4:sts=4:sw=4:et:

