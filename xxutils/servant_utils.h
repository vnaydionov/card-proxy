// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__SERVANT_UTILS_H
#define CARD_PROXY__SERVANT_UTILS_H

#include <string>
#include <stdexcept>

#include <util/data_types.h>
#include <util/nlogger.h>
#include <util/element_tree.h>
#include <orm/data_object.h>

#include "micro_http.h"
#include "utils.h"

void randomize();
const std::string money2str(const Yb::Decimal &x);
const std::string timestamp2str(double ts);
double datetime2timestamp(const Yb::DateTime &d);
double datetime_diff(const Yb::DateTime &a, const Yb::DateTime &b);
const std::string uri_decode(const std::string &s);
const std::string serialize_params(const Yb::StringDict &params);

const std::string dict2str(const Yb::StringDict &params);

class TimerGuard
{
    Yb::ILogger &logger_;
    std::string method_;
    Yb::MilliSec t0_;
    bool except_;
public:
    TimerGuard(Yb::ILogger &logger, const std::string &method = "unknown");
    void set_ok() { except_ = false; }
    void set_method(const std::string &method) { method_ = method; }
    Yb::MilliSec time_spent() const
    {
        return Yb::get_cur_time_millisec() - t0_;
    }
    ~TimerGuard();
};

Yb::ElementTree::ElementPtr mk_resp(
        const std::string &status,
        const std::string &status_code = "");

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

    std::string dump_result(Yb::ElementTree::ElementPtr res);

    const HttpHeaders try_call(TimerGuard &t,
                               const HttpHeaders &request, int n);

public:
    XmlHttpWrapper(): f_(NULL), g_(NULL) {}

    XmlHttpWrapper(const Yb::String &name, XmlHttpHandler f,
            const Yb::String &prefix = _T(""),
            const Yb::String &default_status = _T("internal_error"));

    XmlHttpWrapper(const Yb::String &name, PlainHttpHandler g,
            const Yb::String &prefix = _T(""),
            const Yb::String &default_status = _T("internal_error"));

    const Yb::String &name() const { return name_; }
    const Yb::String &prefix() const { return prefix_; }

    const HttpHeaders operator() (const HttpHeaders &request);
};

#define WRAP(prefix, func) XmlHttpWrapper(_T(#func), func, prefix)

#endif // CARD_PROXY__SERVANT_UTILS_H
// vim:ts=4:sts=4:sw=4:et:
