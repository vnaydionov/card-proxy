// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__SERVANT_UTILS_H
#define CARD_PROXY__SERVANT_UTILS_H

#include <util/data_types.h>
#include <util/element_tree.h>
#include <util/nlogger.h>
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

typedef const std::string (* FilterFunc)(const std::string &value);
typedef std::map<std::string, FilterFunc> FiltersMap;
const std::string mute_param(const std::string &value);
const std::string dict2str(const Yb::StringDict &params,
                           const FiltersMap &filters = FiltersMap());

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


Yb::ElementTree::ElementPtr mk_resp(const Yb::String &status,
        const Yb::String &status_code = _T(""));

class ApiResult: public std::runtime_error
{
    Yb::ElementTree::ElementPtr p_;
    int http_code_;
    std::string http_desc_;
public:
    explicit ApiResult(Yb::ElementTree::ElementPtr p,
                       int http_code = 200, const std::string &http_desc = "OK")
        : runtime_error("api result")
        , p_(p)
        , http_code_(http_code)
        , http_desc_(http_desc)
    {}
    virtual ~ApiResult() throw () {}
    Yb::ElementTree::ElementPtr result() const { return p_; }
    int http_code() const { return http_code_; }
    const std::string &http_desc() const { return http_desc_; }
};

class InvalidParam: public ApiResult
{
    std::string param_;
public:
    explicit InvalidParam(const std::string &param)
        : ApiResult(mk_resp("error", "invalid_param"),
                    400, "Invalid parameter")
        , param_(param)
    {}
    virtual ~InvalidParam() throw () {}
    const std::string &param() const { return param_; }
};

#define ASSERT_PARAM(cond, param) \
    do { if (!(cond)) throw InvalidParam(param); } while(0)


typedef const HttpResponse (*PlainHttpHandler)(
        Yb::ILogger &logger, const HttpRequest &request);

typedef Yb::ElementTree::ElementPtr (*XmlHttpHandler)(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params);


class XmlHttpWrapper
{
    Yb::String name_, prefix_, default_status_, secret_;
    XmlHttpHandler f_;
    PlainHttpHandler g_;
    boost::shared_ptr<Yb::ILogger> logger_;

    std::string dump_result(Yb::ElementTree::ElementPtr res);

    void check_auth(const HttpRequest &request);
    const HttpResponse try_call(TimerGuard &t,
                                const HttpRequest &request, int n);

public:
    XmlHttpWrapper(): f_(NULL), g_(NULL) {}

    XmlHttpWrapper(const Yb::String &name, XmlHttpHandler f,
            const Yb::String &prefix = _T(""),
            const Yb::String &secret_ = _T(""),
            const Yb::String &default_status = _T("internal_error"));

    XmlHttpWrapper(const Yb::String &name, PlainHttpHandler g,
            const Yb::String &prefix = _T(""),
            const Yb::String &secret_ = _T(""),
            const Yb::String &default_status = _T("internal_error"));

    const Yb::String &name() const { return name_; }
    const Yb::String &prefix() const { return prefix_; }

    const HttpResponse operator() (const HttpRequest &request);
    bool is_plain() const { return g_ != NULL; }
};

#define WRAP(prefix, func) XmlHttpWrapper(_T(#func), func, prefix)
#define SECURE_WRAP(prefix, secret, func) XmlHttpWrapper(_T(#func), func, prefix, secret)


#endif // CARD_PROXY__SERVANT_UTILS_H
// vim:ts=4:sts=4:sw=4:et:
