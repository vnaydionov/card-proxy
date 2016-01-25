// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <util/string_utils.h>
#include "micro_http.h"
#include "servant_utils.h"

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

const std::string dict2str(const Yb::StringDict &params)
{
    using Yb::StrUtils::dquote;
    using Yb::StrUtils::c_string_escape;
    std::ostringstream out;
    out << "{";
    Yb::StringDict::const_iterator it = params.begin(), end = params.end();
    for (bool first = true; it != end; ++it, first = false) {
        if (!first)
            out << ", ";
        out << NARROW(it->first) << ": ";
        if (it->first == _T("data"))  // TODO: take this specific to params
            out << NARROW(dquote(_T("...")));
        else
            out << NARROW(dquote(c_string_escape(it->second)));
    }
    out << "}";
    return out.str();
}

void randomize()
{
    std::srand(Yb::get_cur_time_millisec() % 1000000000);
    std::rand();
    std::rand();
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
        const std::string &status,
        const std::string &status_code)
{
    Yb::ElementTree::ElementPtr res =
        Yb::ElementTree::new_element("response");
    res->sub_element("status", status);
    if (!status_code.empty())
        res->sub_element("status_code", status_code);
    char buf[40];
    Yb::MilliSec t = Yb::get_cur_time_millisec();
    std::sprintf(buf, "%u.%03u", (unsigned)(t/1000), (unsigned)(t%1000));
    res->sub_element("ts", buf);
    return res;
}

const std::string uri_decode(const std::string &s)
{
    using Yb::StrUtils::url_decode;
    return url_decode(s);
}

const std::string serialize_params(const Yb::StringDict &params)
{
    return HttpHeaders::serialize_params(params);
}

// vim:ts=4:sts=4:sw=4:et:
