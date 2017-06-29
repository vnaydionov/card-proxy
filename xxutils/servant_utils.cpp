// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <util/string_utils.h>
#include "micro_http.h"
#include "servant_utils.h"
#include "app_class.h"

void randomize()
{
    std::srand(Yb::get_cur_time_millisec() % 1000000000);
    std::rand();
    std::rand();
}

const std::string money2str(const Yb::Decimal &x)
{
    std::ostringstream out;
    out << std::setprecision(2) << x;
    return out.str();
}

const std::string timestamp2str(double ts)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << ts;
    return out.str();
}

double datetime2timestamp(const Yb::DateTime &d)
{
    tm t = boost::posix_time::to_tm(d);
    return (unsigned)std::mktime(&t);
}

double datetime_diff(const Yb::DateTime &a, const Yb::DateTime &b)
{
    boost::posix_time::time_duration td = b - a;
    return td.total_seconds();
}

const std::string uri_decode(const std::string &s)
{
    using Yb::StrUtils::url_decode;
    return url_decode(s);
}

const std::string serialize_params(const Yb::StringDict &params)
{
    return HttpRequest::serialize_params(params);
}

const std::string mute_param(const std::string &value)
{
    return std::string(value.size(), '*');
}

const std::string dict2str(const Yb::StringDict &params,
                           const FiltersMap &filters)
{
    using Yb::StrUtils::dquote;
    using Yb::StrUtils::c_string_escape;
    using Yb::StrUtils::str_to_lower;
    std::ostringstream out;
    out << "{";
    Yb::StringDict::const_iterator it = params.begin(), end = params.end();
    for (bool first = true; it != end; ++it, first = false) {
        if (!first)
            out << ", ";
        std::string key = str_to_lower(it->first);
        std::string value = it->second;
        auto f = filters.find(key);
        if (f != filters.end())
            value = f->second(value);
        out << NARROW(key) << ": " << NARROW(dquote(c_string_escape(value)));
    }
    out << "}";
    return out.str();
}

TimerGuard::TimerGuard(Yb::ILogger &logger, const std::string &method)
    : logger_(logger)
    , method_(method)
    , t0_(Yb::get_cur_time_millisec())
    , except_(true)
{}

TimerGuard::~TimerGuard()
{
    std::ostringstream out;
    out << "Method " << method_ << " ended";
    if (except_)
        out << " with an exception";
    out << ". Execution time=" << time_spent() / 1000.0;
    logger_.info(out.str());
}

Yb::ElementTree::ElementPtr mk_resp(
        const Yb::String &status,
        const Yb::String &status_code)
{
    Yb::ElementTree::ElementPtr res =
        Yb::ElementTree::new_element("response");
    res->sub_element("status", status);
    if (!status_code.empty())
        res->sub_element("status_code", status_code);
    char buf[40];
    Yb::MilliSec t = Yb::get_cur_time_millisec();
    std::snprintf(buf, sizeof(buf), "%u.%03u", (unsigned)(t/1000), (unsigned)(t%1000));
    buf[sizeof(buf) - 1] = 0;
    res->sub_element("ts", buf);
    return res;
}

std::string XmlHttpWrapper::dump_result(Yb::ElementTree::ElementPtr res)
{
    std::string res_str = res->serialize();
    logger_->debug("Method " + NARROW(name_) +
                   ": response: " + res_str);
    return res_str;
}

const HttpResponse XmlHttpWrapper::try_call(TimerGuard &t,
                                            const HttpRequest &request, int n)
{
    logger_->info("Method " + NARROW(name_) +
                  std::string(n? " re": " ") + "started.");
    if (!is_plain())
    {
        const Yb::StringDict &params = request.params();
        logger_->debug("Method " + NARROW(name_) +
                       ": params: " + NARROW(dict2str(params)));
        // call xml wrapped
        std::auto_ptr<Yb::Session> session;
        if (theApp::instance().uses_db())
            session.reset(theApp::instance().new_session().release());
        Yb::ElementTree::ElementPtr res = f_(*session, *logger_, params);
        if (theApp::instance().uses_db())
            session->commit();
        // form the reply
        HttpResponse response(HTTP_1_0, 200, _T("OK"));
        response.set_response_body(dump_result(res), _T("application/xml"));
        t.set_ok();
        return response;
    }
    else
    {
        HttpResponse response = g_(*logger_, request);
        t.set_ok();
        return response;
    }
}

XmlHttpWrapper::XmlHttpWrapper(const Yb::String &name, XmlHttpHandler f,
        const Yb::String &prefix,
        const Yb::String &secret,
        const Yb::String &default_status)
    : name_(name), prefix_(prefix)
    , default_status_(default_status), secret_(secret), f_(f), g_(NULL)
{}

XmlHttpWrapper::XmlHttpWrapper(const Yb::String &name, PlainHttpHandler g,
        const Yb::String &prefix,
        const Yb::String &secret,
        const Yb::String &default_status)
    : name_(name), prefix_(prefix)
    , default_status_(default_status), secret_(secret), f_(NULL), g_(g)
{}

void XmlHttpWrapper::check_auth(const HttpRequest &request)
{
    if (secret_.size()) {
        if (request.get_header("X-AUTH") != secret_) {
            logger_->error("Wrong authentication secret");
            throw ApiResult(mk_resp("no_auth", "wrong_secret"),
                            401, "Unauthorized");
        }
        logger_->debug("Authenticated by secret");
    }
}

const HttpResponse XmlHttpWrapper::operator() (const HttpRequest &request)
{
    logger_.reset(theApp::instance().new_logger(
                f_? "invoker_xml": "invoker_plain").release());
    const Yb::String cont_type = _T("application/xml");
    TimerGuard t(*logger_, NARROW(name_));
    try {
        check_auth(request);

        // do not restart plain methods
        if (is_plain())
            return try_call(t, request, 0);

        // try to restart XML methods in case of DB error
        try {
            return try_call(t, request, 0);
        }
        catch (const Yb::DBError &ex) {
            logger_->error(std::string("db exception: ") + ex.what());
            return try_call(t, request, 1);
        }
    }
    catch (const HttpHeaderNotFound &ex) {
        logger_->error("Missing header: " + ex.header_name());
        HttpResponse response(HTTP_1_0, 400, _T("Bad Request"));
        response.set_response_body(dump_result(mk_resp("error", "missing_header")), cont_type);
        return response;
    }
    catch (const ApiResult &ex) {
        if (ex.http_code() == 200)
            t.set_ok();
        HttpResponse response(HTTP_1_0, ex.http_code(), ex.http_desc());
        response.set_response_body(dump_result(ex.result()), cont_type);
        return response;
    }
    catch (const std::exception &ex) {
        logger_->error(std::string("exception: ") + ex.what());
        HttpResponse response(HTTP_1_0, 500, _T("Internal error"));
        response.set_response_body(dump_result(mk_resp(default_status_)), cont_type);
        return response;
    }
    catch (...) {
        logger_->error("unknown exception");
        HttpResponse response(HTTP_1_0, 500, _T("Internal error"));
        response.set_response_body(dump_result(mk_resp(default_status_)), cont_type);
        return response;
    }
}

// vim:ts=4:sts=4:sw=4:et:
