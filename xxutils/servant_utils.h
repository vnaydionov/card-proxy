// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__SERVANT_UTILS_H
#define CARD_PROXY__SERVANT_UTILS_H

#include <string>
#include <stdexcept>

#include <util/data_types.h>
#include <util/nlogger.h>
#include <util/element_tree.h>

#include "utils.h"

const std::string money2str(const Yb::Decimal &x);
const std::string timestamp2str(double ts);
double datetime2timestamp(const Yb::DateTime &d);
double datetime_diff(const Yb::DateTime &a, const Yb::DateTime &b);
void randomize();
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

#endif // CARD_PROXY__SERVANT_UTILS_H
// vim:ts=4:sts=4:sw=4:et:
