#include "helpers.h"
#include <util/string_utils.h>

using namespace std;
using namespace Yb;

const string money2str(const Decimal &x)
{
    ostringstream out;
    out << setprecision(2) << x;
    return out.str();
}

const string timestamp2str(double ts)
{
    ostringstream out;
    out << fixed << setprecision(3) << ts;
    return out.str();
}

double datetime2timestamp(const DateTime &d)
{
    tm t = boost::posix_time::to_tm(d);
    return (unsigned)mktime(&t);
}

double datetime_diff(const DateTime &a, const DateTime &b)
{
    boost::posix_time::time_duration td = b - a;
    return td.total_seconds();
}

const string dict2str(const StringDict &params)
{
    using namespace StrUtils;
    ostringstream out;
    out << "{";
    StringDict::const_iterator it = params.begin(), end = params.end();
    for (bool first = true; it != end; ++it, first = false) {
        if (!first)
            out << ", ";
        out << it->first << ": " << dquote(c_string_escape(it->second));
    }
    out << "}";
    return out.str();
}

const StringDict json2dict(const string &json_str) {
    using namespace StrUtils;
    StringDict r;
    string key, value;
    enum { OutOfBraces, InBraces, InKey, AfterKey, AfterColon,
           InStrValue, InValue, AfterValue } st = OutOfBraces;
    for (size_t i = 0; i < json_str.size(); ++i) {
        char c = json_str[i];
        if (st == OutOfBraces) {
            if (c == '{') {
                st = InBraces;
            }
            else {
                YB_ASSERT(is_space(c));
            }
        }
        else if (st == InBraces) {
            if (c == '"') {
                st = InKey;
                key = "";
            }
            else if (c == '}') {
                st = OutOfBraces;
            }
            else {
                YB_ASSERT(is_space(c));
            }
        }
        else if (st == InKey) {
            if (c == '"') {
                st = AfterKey;
            }
            else {
                YB_ASSERT(c != '\\');
                key += c;
            }
        }
        else if (st == AfterKey) {
            if (c == ':') {
                st = AfterColon;
            }
            else {
                YB_ASSERT(is_space(c));
            }
        }
        else if (st == AfterColon) {
            if (c == '"') {
                st = InStrValue;
                value = "";
            }
            else if (!is_space(c)) {
                YB_ASSERT(c != '{');
                st = InValue;
                value = c;
            }
        }
        else if (st == InStrValue) {
            if (c == '"') {
                st = AfterValue;
                r[key] = value;
            }
            else {
                ///
                YB_ASSERT(c != '\\');
                value += c;
            }
        }
        else if (st == InValue) {
            if (is_space(c) || c == ',' || c == '}') {
                st = AfterValue;
                r[key] = value;
                --i;
            }
            else {
                ///
                YB_ASSERT(c != '\\');
                value += c;
            }
        }
        else if (st == AfterValue) {
            if (c == ',') {
                st = InBraces;
            }
            else if (c == '}') {
                st = OutOfBraces;
            }
            else {
                YB_ASSERT(is_space(c));
            }
        }
    }
    YB_ASSERT(st == OutOfBraces);
    return r;
}

void randomize()
{
    srand(get_cur_time_millisec() % 1000000000);
    rand();
    rand();
}

TimerGuard::~TimerGuard()
{
    ostringstream out;
    out << "finished";
    if (except_)
        out << " with an exception";
    out << ", " << time_spent() << " ms";
    logger_.info(out.str());
}

// vim:ts=4:sts=4:sw=4:et:
