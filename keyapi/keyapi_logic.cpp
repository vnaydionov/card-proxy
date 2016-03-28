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

Yb::ElementTree::ElementPtr KeyAPI::generate_hmac(const Yb::StringDict &params)
{
    IConfig &config(theApp::instance().cfg());
    config.reload();
    TokenizerConfig tcfg;
    auto password = params.get("password", "");
    auto r = kek_auth(tcfg, "CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);

    const auto hmac_versions = tcfg.get_hmac_versions();
    auto k = std::max_element(hmac_versions.begin(), hmac_versions.end());
    YB_ASSERT(hmac_versions.end() != k);
    int new_hmac_version = *k + 1;
    const auto &master_key = tcfg.get_master_key(kek_version);
    DEKPool dek_pool(theApp::instance().cfg(), *log_, session_,
                     master_key, kek_version);
    Domain::DataKey new_hmac_key = dek_pool.generate_new_data_key(true);
    session_.flush();
    set_config_param("HMAC_VER" + Yb::to_string(new_hmac_version)
                     + "_ID", Yb::to_string(new_hmac_key.id.value()));
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("new_hmac_version", Yb::to_string(new_hmac_version));
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::generate_kek(const Yb::StringDict &params)
{
    IConfig &config(theApp::instance().cfg());
    config.reload();
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
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::get_component(const Yb::StringDict &params)
{
    IConfig &config(theApp::instance().cfg());
    config.reload();
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
    IConfig &config(theApp::instance().cfg());
    config.reload();
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
    IConfig &config(theApp::instance().cfg());
    config.reload();
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
    IConfig &config(theApp::instance().cfg());
    config.reload();
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

    int hmac_version = tcfg.get_active_hmac_key_version();
    cmap = get_hmac_use_counts();
    auto hmac_versions = tcfg.get_hmac_versions();
    for (auto i = hmac_versions.begin(), iend = hmac_versions.end();
            i != iend; ++i)
    {
        if (hmac_version == *i)
            continue;
        int use_count = 0;
        auto j = cmap.find(*i);
        if (j != cmap.end())
            use_count = boost::lexical_cast<int>(j->second);
        if (use_count > 0)
            continue;
        Yb::ElementTree::ElementPtr kek_node = purged_keys->sub_element("hmac");
        kek_node->attrib_["version"] = Yb::to_string(*i);
        std::string prefix = "HMAC_VER" + Yb::to_string(*i) + "_";
        unset_config_param(prefix + "ID");
        session_.flush();
    }
    return resp;
}

void KeyAPI::rehash_token(TokenizerConfig &tcfg, Domain::DataToken &data_token,
                          Domain::DataKey &data_key, int target_hmac_version)
{
    auto master_key = tcfg.get_master_key(data_key.kek_version);
    AESCrypter c1(master_key);
    auto dek = c1.decrypt(decode_base64(data_key.dek_crypted));
    AESCrypter c2(dek);
    auto data = c2.decrypt(decode_base64(data_token.data_crypted));
    auto new_hmac_key = tcfg.get_hmac_key(target_hmac_version);
    data_token.hmac_digest = Tokenizer::count_hmac(data, new_hmac_key);
    data_token.hmac_version = target_hmac_version;
}

Yb::ElementTree::ElementPtr KeyAPI::rehash_tokens(const Yb::StringDict &params)
{
    IConfig &config(theApp::instance().cfg());
    config.reload();
    TokenizerConfig tcfg;
    auto j = params.find("id_min");
    YB_ASSERT(j != params.end());
    const auto &id_min = j->second;
    j = params.find("id_max");
    YB_ASSERT(j != params.end());
    const auto &id_max = j->second;
    int target_hmac_version = tcfg.get_active_hmac_key_version();
    typedef boost::tuple<Domain::DataToken, Domain::DataKey> Pair;
    auto rs = Yb::query<Pair>(session_)
        .select_from<Domain::DataToken>()
        .join<Domain::DataKey>()
        .filter_by(Domain::DataToken::c.hmac_version != target_hmac_version)
        .filter_by(Domain::DataToken::c.id >= id_min)
        .filter_by(Domain::DataToken::c.id <= id_max)
        .for_update()
        .all();
    int converted = 0, failed = 0;
    for (auto i = rs.begin(), iend = rs.end(); i != iend; ++i)
    {
        Domain::DataToken &data_token = i->get<0>();
        Domain::DataKey &data_key = i->get<1>();
        rehash_token(tcfg, data_token, data_key, target_hmac_version);
        ++converted;
    }
    session_.commit();
    auto resp = mk_resp();
    resp->sub_element("target_hmac_version", Yb::to_string(target_hmac_version));
    resp->sub_element("id_min", Yb::to_string(id_min));
    resp->sub_element("id_max", Yb::to_string(id_max));
    resp->sub_element("converted", Yb::to_string(converted));
    resp->sub_element("failed", Yb::to_string(failed));
    return resp;
}

void KeyAPI::reencrypt_dek(TokenizerConfig &tcfg, Domain::DataKey &data_key,
                           int target_kek_version)
{
    auto old_master_key = tcfg.get_master_key(data_key.kek_version);
    auto new_master_key = tcfg.get_master_key(target_kek_version);
    AESCrypter c1(old_master_key);
    auto dek = c1.decrypt(decode_base64(data_key.dek_crypted));
    AESCrypter c2(new_master_key);
    data_key.dek_crypted = encode_base64(c2.encrypt(dek));
    data_key.kek_version = target_kek_version;
}

Yb::ElementTree::ElementPtr KeyAPI::reencrypt_deks(const Yb::StringDict &params)
{
    IConfig &config(theApp::instance().cfg());
    config.reload();
    TokenizerConfig tcfg;
    auto j = params.find("id_min");
    YB_ASSERT(j != params.end());
    const auto &id_min = j->second;
    j = params.find("id_max");
    YB_ASSERT(j != params.end());
    const auto &id_max = j->second;
    int target_kek_version = tcfg.get_switch_version();
    auto rs = Yb::query<Domain::DataKey>(session_)
        .filter_by(Domain::DataKey::c.kek_version != target_kek_version)
        .filter_by(Domain::DataKey::c.id >= id_min)
        .filter_by(Domain::DataKey::c.id <= id_max)
        .for_update()
        .all();
    int converted = 0, failed = 0;
    for (auto i = rs.begin(), iend = rs.end(); i != iend; ++i)
    {
        Domain::DataKey &data_key = *i;
        reencrypt_dek(tcfg, data_key, target_kek_version);
        ++converted;
    }
    session_.commit();
    auto resp = mk_resp();
    resp->sub_element("target_kek_version", Yb::to_string(target_kek_version));
    resp->sub_element("id_min", Yb::to_string(id_min));
    resp->sub_element("id_max", Yb::to_string(id_max));
    resp->sub_element("converted", Yb::to_string(converted));
    resp->sub_element("failed", Yb::to_string(failed));
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::switch_hmac(const Yb::StringDict &params)
{
    IConfig &config(theApp::instance().cfg());
    config.reload();
    TokenizerConfig tcfg;
    std::string mode = "CURRENT";
    auto password = params.get("password", "");
    auto r = kek_auth(tcfg, mode, password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);

    const auto hmac_versions = tcfg.get_hmac_versions();
    auto k = std::max_element(hmac_versions.begin(), hmac_versions.end());
    YB_ASSERT(hmac_versions.end() != k);
    int new_hmac_version = *k;
    int old_hmac_version = tcfg.get_active_hmac_key_version();
    YB_ASSERT(new_hmac_version != old_hmac_version);
    set_config_param("HMAC_VERSION", Yb::to_string(new_hmac_version));
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("old_hmac_version", Yb::to_string(old_hmac_version));
    resp->sub_element("new_hmac_version", Yb::to_string(new_hmac_version));
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::switch_kek(const Yb::StringDict &params)
{
    IConfig &config(theApp::instance().cfg());
    config.reload();
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
    IConfig &config(theApp::instance().cfg());
    config.reload();
    TokenizerConfig tcfg;
    auto resp = mk_resp();
    resp->sub_element("keyapi_state",
        tcfg.get_db_config_key("STATE"));
    resp->sub_element("hmac_version",
        Yb::to_string(tcfg.get_active_hmac_key_version()));
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
