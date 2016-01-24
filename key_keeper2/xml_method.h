#ifndef _AUTH__XML_METHOD_H_
#define _AUTH__XML_METHOD_H_

#include <util/string_utils.h>
#include <util/element_tree.h>
#include "micro_http.h"

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
    ~TimerGuard() {
        std::ostringstream out;
        out << "finished";
        if (except_)
            out << " with an exception";
        out << ", " << time_spent() << " ms";
        logger_.info(out.str());
    }
};

inline Yb::ElementTree::ElementPtr mk_resp(const Yb::String &status,
        const Yb::String &status_code = _T(""))
{
    Yb::ElementTree::ElementPtr res = Yb::ElementTree::new_element(_T("result"));
    res->sub_element(_T("status"), status);
    if (!Yb::str_empty(status_code))
        res->sub_element(_T("status_code"), status_code);
    return res;
}

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

inline const std::string dict2str(const Yb::StringDict &params)
{
    using namespace Yb::StrUtils;
    std::ostringstream out;
    out << "{";
    Yb::StringDict::const_iterator it = params.begin(), end = params.end();
    for (bool first = true; it != end; ++it, first = false) {
        if (!first)
            out << ", ";
        out << NARROW(it->first) << ": ";
        if (it->first == _T("data"))
            out << NARROW(dquote(_T("...")));
        else
            out << NARROW(dquote(c_string_escape(it->second)));
    }
    out << "}";
    return out.str();
}

typedef const HttpHeaders (*PlainHttpHandler)(
        Yb::ILogger &logger, const HttpHeaders &request);

typedef Yb::ElementTree::ElementPtr (*XmlHttpHandler)(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params);

class XmlHttpWrapper
{
    Yb::String name_, prefix_, default_status_;
    XmlHttpHandler f_;

    std::string dump_result(Yb::ILogger &logger, Yb::ElementTree::ElementPtr res)
    {
        std::string res_str = res->serialize();
        logger.debug("result: " + res_str);
        return res_str;
    }

public:
    XmlHttpWrapper(): f_(NULL) {}

    XmlHttpWrapper(const Yb::String &name, XmlHttpHandler f,
            const Yb::String &prefix = _T(""),
            const Yb::String &default_status = _T("internal_error"))
        : name_(name), prefix_(prefix), default_status_(default_status), f_(f)
    {}

    const Yb::String &name() const { return name_; }
    const Yb::String &prefix() const { return prefix_; }

    const HttpHeaders operator() (const HttpHeaders &request)
    {
        const Yb::String cont_type = _T("application/xml");
        Yb::ILogger::Ptr logger(theApp::instance().new_logger(_T("method_wrapper")));
        TimerGuard t(*logger);
        try {
            const Yb::StringDict &params = request.get_params();
            logger->info("started path: " + NARROW(request.get_path())
                         + ", params: " + dict2str(params));
            std::auto_ptr<Yb::Session> session(
                    theApp::instance().new_session());
            Yb::ElementTree::ElementPtr res = f_(*session, *logger, params);
            session->commit();
            HttpHeaders response(10, 200, _T("OK"));
            response.set_response_body(dump_result(*logger, res), cont_type);
            t.set_ok();
            return response;
        }
        catch (const ApiResult &ex) {
            t.set_ok();
            HttpHeaders response(10, 200, _T("OK"));
            response.set_response_body(dump_result(*logger, ex.result()), cont_type);
            return response;
        }
        catch (const std::exception &ex) {
            logger->error(std::string("exception: ") + ex.what());
            HttpHeaders response(10, 500, _T("Internal error"));
            response.set_response_body(dump_result(*logger, mk_resp(default_status_)), cont_type);
            return response;
        }
        catch (...) {
            logger->error("unknown exception");
            HttpHeaders response(10, 500, _T("Internal error"));
            response.set_response_body(dump_result(*logger, mk_resp(default_status_)), cont_type);
            return response;
        }
    }
};

#define WRAP(prefix, func) XmlHttpWrapper(_T(#func), func, prefix)

#endif // _AUTH__XML_METHOD_H_
// vim:ts=4:sts=4:sw=4:et:

