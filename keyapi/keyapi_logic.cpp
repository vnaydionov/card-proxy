// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "keyapi_logic.h"
#include "utils.h"
#include "app_class.h"
#include "tokenizer.h"
#include <boost/regex.hpp>
#include <stdio.h>

#include "domain/Config.h"

double KeyAPI::get_time()
{
    return Yb::get_cur_time_millisec()/1000.;
}

const std::string KeyAPI::format_ts(double ts)
{
    char buf[40];
    sprintf(buf, "%.3lf", ts);
    return std::string(buf);
}

static Yb::ElementTree::ElementPtr find_item(
        Yb::ElementTree::ElementPtr items, const std::string &id)
{
    auto found = items->find_all("item");
    for (auto i = found->begin(), iend = found->end(); i != iend; ++i)
    {
        if ((*i)->attrib_.get("id", "") == id)
            return *i;
    }
    Yb::ElementTree::ElementPtr null;
    return null;
}


KeyAPI::KeyAPI(IConfig &cfg, Yb::ILogger &log, Yb::Session &session)
    : log_(log.new_logger("keyapi").release())
    , session_(session)
{}

const std::string KeyAPI::get_config_param(const std::string &key)
{
    auto rec = Yb::query<Domain::Config>(session_)
        .filter_by(Domain::Config::c.ckey == key)
        .one();
    return rec.cvalue;
}

void KeyAPI::set_config_param(const std::string &key, const std::string &value)
{
    Domain::Config rec;
    try {
        rec = Yb::query<Domain::Config>(session_)
            .filter_by(Domain::Config::c.ckey == key)
            .one();
    }
    catch (const Yb::NoDataFound &) {
        rec.ckey = key;
        rec.save(session_);
    }
    rec.cvalue = value;
    session_.flush();
}

const std::string KeyAPI::get_state()
{
    return get_config_param("STATE");
}

void KeyAPI::set_state(const std::string &state)
{
    set_config_param("STATE", state);
}

std::pair<int, int> KeyAPI::kek_auth(const std::string &mode,
        const std::string &password, int kek_version)
{
    YB_ASSERT(password.size() == 64);
    YB_ASSERT(mode == "CURRENT" || mode == "LAST" ||
              mode == "TARGET" || mode == "CUSTOM");
    TokenizerConfig tcfg;
    if (mode == "CURRENT")
        kek_version = tcfg.get_current_version();
    else if (mode == "LAST")
        kek_version = tcfg.get_last_version();
    else
        kek_version = tcfg.get_switch_version();
    for (int i = 1; i <= 3; ++i) {
        if (tcfg.get_master_key_component(kek_version, i) == password)
            return std::make_pair(kek_version, i);
    }
    return std::make_pair(kek_version, 0);
}

Yb::ElementTree::ElementPtr KeyAPI::mk_resp(const std::string &status)
{
    auto root = Yb::ElementTree::new_element("result");
    root->sub_element("status", status);
    root->sub_element("ts", format_ts(get_time()));
    return root;
}

Yb::ElementTree::ElementPtr KeyAPI::generate_kek(const Yb::StringDict &params)
{
    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::get_component(const Yb::StringDict &params)
{
    auto password = params.get("password", "");
    auto want_kek_version_str = params.get("version", "");
    auto r = kek_auth("CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    TokenizerConfig tcfg;
    int want_kek_version = -1;
    if (!want_kek_version_str.empty())
        want_kek_version = boost::lexical_cast<int>(want_kek_version_str);
    else
        want_kek_version = tcfg.get_last_version();
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("show_version", Yb::to_string(want_kek_version));
    resp->sub_element("component",
            tcfg.get_master_key_component(want_kek_version, part_n));
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::confirm_component(const Yb::StringDict &params)
{
    auto password = params.get("password", "");
    auto want_kek_version_str = params.get("version", "");
    std::string mode = "LAST";
    int want_kek_version = -1;
    if (!want_kek_version_str.empty()) {
        mode = "CUSTOM";
        want_kek_version = boost::lexical_cast<int>(want_kek_version_str);
    }
    auto r = kek_auth(mode, password, want_kek_version);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    set_config_param("KEK_VER" + Yb::to_string(kek_version) +
            "_PART" + Yb::to_string(part_n) + "_CHECK", "1");
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::reset_target_version(const Yb::StringDict &params)
{
    auto password = params.get("password", "");
    int want_kek_version = boost::lexical_cast<int>(params.get("version", ""));
    auto r = kek_auth("CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    TokenizerConfig tcfg;
    YB_ASSERT(tcfg.is_kek_valid(want_kek_version));
    YB_ASSERT(tcfg.is_version_checked(want_kek_version));
    set_config_param("KEK_TARGET_VERSION", Yb::to_string(want_kek_version));
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("target_version", Yb::to_string(want_kek_version));
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::cleanup(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;

    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::reencrypt_deks(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;

    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::switch_kek(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;

    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::status(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;

    return mk_resp();
}

// vim:ts=4:sts=4:sw=4:et:
