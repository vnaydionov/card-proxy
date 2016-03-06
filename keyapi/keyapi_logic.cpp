// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "keyapi_logic.h"
#include "utils.h"
#include "app_class.h"
#include "aes_crypter.h"
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

void KeyAPI::unset_config_param(const std::string &key)
{
    try {
        Domain::Config rec = Yb::query<Domain::Config>(session_)
            .filter_by(Domain::Config::c.ckey == key)
            .one();
        rec.delete_object();
    }
    catch (const Yb::NoDataFound &) {
    }
}

const std::string KeyAPI::get_state()
{
    return get_config_param("STATE");
}

void KeyAPI::set_state(const std::string &state)
{
    set_config_param("STATE", state);
}

std::pair<int, int> KeyAPI::kek_auth(
        TokenizerConfig &tcfg, const std::string &mode,
        const std::string &password, int kek_version)
{
    YB_ASSERT(password.size() == 64);
    YB_ASSERT(mode == "CURRENT" || mode == "LAST" ||
              mode == "TARGET" || mode == "CUSTOM");
    if (mode == "CURRENT")
        kek_version = tcfg.get_current_version();
    else if (mode == "LAST")
        kek_version = tcfg.get_last_version();
    else
        kek_version = boost::lexical_cast<int>(
                tcfg.get_db_config_key("KEK_TARGET_VERSION"));
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
    TokenizerConfig tcfg;
    auto password = params.get("password", "");
    auto r = kek_auth(tcfg, "CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);

    std::string kek1_hex = generate_random_hex_string(64);
    std::string kek2_hex = generate_random_hex_string(64);
    std::string kek3_hex = generate_random_hex_string(64);
    auto kek = TokenizerConfig::assemble_kek(kek1_hex, kek2_hex, kek3_hex);
    AESCrypter aes_crypter(kek);
    auto control_phrase = tcfg.get_db_config_key("KEK_CONTROL_PHRASE");
    auto control_code = encode_base64(aes_crypter.encrypt(control_phrase));
    int new_kek_version = tcfg.get_last_version() + 1;
    std::string prefix = "KEK_VER" + Yb::to_string(new_kek_version) + "_";
    set_config_param(prefix + "PART3", kek3_hex);
    set_config_param(prefix + "CONTROL_CODE", control_code);
    set_config_param(prefix + "PART1_CHECK", "0");
    set_config_param(prefix + "PART2_CHECK", "0");
    set_config_param(prefix + "PART3_CHECK", "0");
    set_config_param("KEK_TARGET_VERSION", Yb::to_string(new_kek_version));
    session_.flush();

    IConfig &cfg(theApp::instance().cfg());
    auto kk_config = get_keykeeper_controller(cfg);
    KeyKeeperAPI kk_api(kk_config.get<0>(), kk_config.get<1>(), 1, log_.get());
    kk_api.send_key_to_server(kek1_hex, new_kek_version);

    auto cpatch_timeout = cfg.get_value_as_int("ConfPatch/Timeout")/1000.;
    for (int i = 1; i <= 6; ++i) {
        std::string cpatch_uri;
        try {
            const std::string key = "ConfPatch/URL" + Yb::to_string(i);
            cpatch_uri = cfg.get_value(key);
        }
        catch (const std::exception &) {
            continue;
        }
        bool ssl_validate = false;  // TODO: fix for prod
        KeyKeeperAPI cpatch_api(cpatch_uri, cpatch_timeout, 2, log_.get(),
                                ssl_validate);
        cpatch_api.send_key_to_server(kek2_hex, new_kek_version);
    }

    tcfg.refresh(true);
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("new_version", Yb::to_string(new_kek_version));
    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::get_component(const Yb::StringDict &params)
{
    TokenizerConfig tcfg;
    auto password = params.get("password", "");
    auto want_kek_version_str = params.get("version", "");
    auto r = kek_auth(tcfg, "CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
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
    TokenizerConfig tcfg;
    auto password = params.get("password", "");
    auto want_kek_version_str = params.get("version", "");
    std::string mode = "TARGET";
    int want_kek_version = -1;
    if (!want_kek_version_str.empty()) {
        mode = "CUSTOM";
        want_kek_version = boost::lexical_cast<int>(want_kek_version_str);
    }
    auto r = kek_auth(tcfg, mode, password, want_kek_version);
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
    TokenizerConfig tcfg;
    auto password = params.get("password", "");
    int want_kek_version = boost::lexical_cast<int>(params.get("version", ""));
    auto r = kek_auth(tcfg, "CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    YB_ASSERT(tcfg.is_kek_valid(want_kek_version));
    YB_ASSERT(tcfg.is_version_checked(want_kek_version));
    set_config_param("KEK_TARGET_VERSION", Yb::to_string(want_kek_version));
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("target_version", Yb::to_string(want_kek_version));
    return resp;
}

void KeyAPI::cleanup_kek(int kek_version)
{
    IConfig &cfg(theApp::instance().cfg());
    auto kk_config = get_keykeeper_controller(cfg);
    KeyKeeperAPI kk_api(kk_config.get<0>(), kk_config.get<1>(), 1, log_.get());
    kk_api.cleanup(kek_version);
    auto cpatch_timeout = cfg.get_value_as_int("ConfPatch/Timeout")/1000.;
    for (int i = 1; i <= 6; ++i) {
        std::string cpatch_uri;
        try {
            const std::string key = "ConfPatch/URL" + Yb::to_string(i);
            cpatch_uri = cfg.get_value(key);
        }
        catch (const std::exception &) {
            continue;
        }
        bool ssl_validate = false;  // TODO: fix for prod
        KeyKeeperAPI cpatch_api(cpatch_uri, cpatch_timeout, 2, log_.get(),
                                ssl_validate);
        cpatch_api.cleanup(kek_version);
    }
}

Yb::ElementTree::ElementPtr KeyAPI::cleanup(const Yb::StringDict &params)
{
    TokenizerConfig tcfg;
    std::string mode = "CURRENT";
    auto password = params.get("password", "");
    auto r = kek_auth(tcfg, mode, password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    int old_kek_version = tcfg.get_active_master_key_version();
    YB_ASSERT(kek_version == old_kek_version);
    int target_kek_version = -1;
    try {
        target_kek_version = boost::lexical_cast<int>(
                tcfg.get_db_config_key("KEK_TARGET_VERSION"));
    }
    catch (const std::exception &)
    {}
    YB_ASSERT(target_kek_version == kek_version);

    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    Yb::ElementTree::ElementPtr purged_keys =
        resp->sub_element("purged_keys");
    VersionMap cmap = get_kek_use_counts();
    auto kek_versions = tcfg.get_versions();
    for (auto i = kek_versions.begin(), iend = kek_versions.end();
            i != iend; ++i)
    {
        if (kek_version == *i)
            continue;
        int use_count = 0;
        auto j = cmap.find(*i);
        if (j != cmap.end())
            use_count = boost::lexical_cast<int>(j->second);
        YB_ASSERT(0 == use_count);
        Yb::ElementTree::ElementPtr kek_node = purged_keys->sub_element("kek");
        kek_node->attrib_["version"] = Yb::to_string(*i);
        std::string prefix = "KEK_VER" + Yb::to_string(*i) + "_";
        unset_config_param(prefix + "PART3");
        unset_config_param(prefix + "CONTROL_CODE");
        unset_config_param(prefix + "PART1_CHECK");
        unset_config_param(prefix + "PART2_CHECK");
        unset_config_param(prefix + "PART3_CHECK");
        session_.flush();
    }
    cleanup_kek(kek_version);
    tcfg.refresh(true);
    return resp;
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
    TokenizerConfig tcfg;
    std::string mode = "TARGET";
    auto password = params.get("password", "");
    auto r = kek_auth(tcfg, mode, password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    int old_kek_version = tcfg.get_active_master_key_version();
    YB_ASSERT(kek_version != old_kek_version);
    YB_ASSERT(tcfg.is_kek_valid(kek_version));
    YB_ASSERT(tcfg.is_version_checked(kek_version));
    set_config_param("KEK_VERSION", Yb::to_string(kek_version));
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("old_version", Yb::to_string(old_kek_version));
    return resp;
}

// load use counts for each of kek versions
const VersionMap KeyAPI::get_kek_use_counts()
{
    VersionMap cmap;
    auto rs = session_.engine()->select_iter(
        Yb::SelectExpr(Yb::ExpressionList(
            Domain::DataKey::c.kek_version,
            Yb::ColumnExpr("count(*) cnt")))
        .from_(Yb::Expression(Domain::DataKey::get_table_name()))
        .group_by_(Domain::DataKey::c.kek_version));
    for (auto i = rs.begin(), iend = rs.end();
            i != iend; ++i)
    {
        log_->debug("kek_count[" + (*i)[0].second.as_string() + 
                    "]=" + (*i)[1].second.as_string());
        cmap[(*i)[0].second.as_integer()] = (*i)[1].second.as_string();
    }
    return cmap;
}

// load use counts for each of hmac versions
const VersionMap KeyAPI::get_hmac_use_counts()
{
    VersionMap cmap;
    auto rs = session_.engine()->select_iter(
        Yb::SelectExpr(Yb::ExpressionList(
            Domain::DataToken::c.hmac_version,
            Yb::ColumnExpr("count(*) cnt")))
        .from_(Yb::Expression(Domain::DataToken::get_table_name()))
        .group_by_(Domain::DataToken::c.hmac_version));
    for (auto i = rs.begin(), iend = rs.end();
            i != iend; ++i)
    {
        log_->debug("hmac_count[" + (*i)[0].second.as_string() + 
                    "]=" + (*i)[1].second.as_string());
        cmap[(*i)[0].second.as_integer()] = (*i)[1].second.as_string();
    }
    return  cmap;
}

Yb::ElementTree::ElementPtr KeyAPI::status(const Yb::StringDict &params)
{
    TokenizerConfig tcfg;
    auto resp = mk_resp();
    resp->sub_element("keyapi_state",
        tcfg.get_db_config_key("STATE"));
    resp->sub_element("kek_version",
        Yb::to_string(tcfg.get_active_master_key_version()));
    try {
        resp->sub_element("target_kek_version",
            tcfg.get_db_config_key("KEK_TARGET_VERSION"));
    }
    catch (const std::exception &)
    {}

    VersionMap cmap = get_kek_use_counts();
    Yb::ElementTree::ElementPtr master_keys =
        resp->sub_element("master_keys");
    auto kek_versions = tcfg.get_versions(true);
    for (auto i = kek_versions.begin(), iend = kek_versions.end();
            i != iend; ++i)
    {
        Yb::ElementTree::ElementPtr master_key =
            master_keys->sub_element("kek");
        master_key->attrib_["version"] = Yb::to_string(*i);
        master_key->attrib_["valid"] = tcfg.is_kek_valid(*i)? "true": "false";
        master_key->attrib_["checked"] = tcfg.is_version_checked(*i)? "true": "false";
        auto j = cmap.find(*i);
        if (j != cmap.end())
            master_key->attrib_["count"] = j->second;
        else
            master_key->attrib_["count"] = "0";
    }

    cmap = get_hmac_use_counts();
    Yb::ElementTree::ElementPtr hmac_keys =
        resp->sub_element("hmac_keys");
    auto hmac_versions = tcfg.get_hmac_versions();
    for (auto i = hmac_versions.begin(), iend = hmac_versions.end();
            i != iend; ++i)
    {
        Yb::ElementTree::ElementPtr hmac_key =
            hmac_keys->sub_element("hmac");
        hmac_key->attrib_["version"] = Yb::to_string(*i);
        auto j = cmap.find(*i);
        if (j != cmap.end())
            hmac_key->attrib_["count"] = j->second;
        else
            hmac_key->attrib_["count"] = "0";
    }
    return resp;
}

// vim:ts=4:sts=4:sw=4:et:
