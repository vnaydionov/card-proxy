// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <util/string_utils.h>
#include "tokenizer.h"
#include "utils.h"
#include "http_post.h"
#include "aes_crypter.h"
#include "dek_pool.h"
#include "tcp_socket.h"
#include "app_class.h"

#include "domain/DataKey.h"
#include "domain/Config.h"
#include "domain/DataToken.h"
#include "domain/SecureVault.h"

using Yb::StrUtils::starts_with;
using Yb::StrUtils::ends_with;


boost::tuple<std::string, double, int, std::string> get_keykeeper_controller(
        IConfig &config)
{
    std::string uri = config.get_value("KeyKeeper2/URL");
    double timeout =
        config.get_value_as_int("KeyKeeper2/Timeout")/1000.;
    int part = 1;
    std::string secret = config.get_value("KK2Secret");
    return boost::make_tuple(uri, timeout, part, secret);
}

const HttpHeaders KeyKeeperAPI::get_headers() const{
    HttpHeaders res;
    res["X-AUTH"] = secret_;
    return res;
}

void KeyKeeperAPI::set_secret(const Yb::String &secret){
    secret_ = secret;
}

const std::string
    KeyKeeperAPI::recv_key_from_server(int kek_version)
{
    double key_keeper_timeout = timeout_;
    std::string key_keeper_uri = uri_;
    std::string target_id = get_target_id(kek_version);
    HttpResponse resp = http_post(key_keeper_uri + "read",
                                  logger_,
                                  key_keeper_timeout,
                                  "GET",
                                  get_headers(), HttpParams(), "",
                                  ssl_validate_cert_);
    validate_status(resp.resp_code());
    const std::string &body = resp.body();
    auto root = Yb::ElementTree::parse(body);
    if (root->find_first("status")->get_text() != "success")
        throw ::RunTimeError("recv_key_from_server: not success");
    std::string result;
    bool found = false;
    const std::string suffix = "_PART" + Yb::to_string(part_);
    auto items_node = root->find_first("items");
    auto item_nodes = items_node->find_children("item");
    auto i = item_nodes->begin(), iend = item_nodes->end();
    for (; i != iend; ++i) {
        auto &node = *i;
        const std::string &id = node->attrib_["id"];
        cached_[id] = node->attrib_["data"];
        if (!starts_with(id, "KEK_VER") || !ends_with(id, suffix))
            continue;
        if (id == target_id) {
            result = node->attrib_["data"];
            found = true;
        }
    }
    if (!found)
        throw ::RunTimeError("recv_key_from_server: key not found");
    return result;
}

const std::string &
    KeyKeeperAPI::get_key_by_version(int kek_version)
{
    std::string target_id = get_target_id(kek_version);
    auto i = cached_.find(target_id);
    if (cached_.end() == i) {
        recv_key_from_server(kek_version);
        i = cached_.find(target_id);
    }
    return i->second;
}

void KeyKeeperAPI::send_key_to_server(const std::string &key,
                                      int kek_version)
{
    double key_keeper_timeout = timeout_;
    std::string key_keeper_uri = uri_;
    if (kek_version < 0)
        kek_version = 0;
    std::string target_id = get_target_id(kek_version);
    HttpParams params;
    params["id"] = target_id;
    params["data"] = key;
    HttpResponse resp = http_post(key_keeper_uri + "set",
                                  logger_,
                                  key_keeper_timeout,
                                  "POST",
                                  get_headers(),
                                  params,
                                  "",
                                  ssl_validate_cert_);
    validate_status(resp.resp_code());
    validate_body(resp.body());
}

void KeyKeeperAPI::cleanup(int kek_version)
{
    double key_keeper_timeout = timeout_;
    std::string key_keeper_uri = uri_;
    if (kek_version < 0)
        kek_version = 0;
    std::string target_id = get_target_id(kek_version);
    HttpParams params;
    params["id"] = target_id;
    HttpResponse resp = http_post(key_keeper_uri + "cleanup",
                                  logger_,
                                  key_keeper_timeout,
                                  "POST",
                                  get_headers(),
                                  params,
                                  "",
                                  ssl_validate_cert_);
    validate_status(resp.resp_code());
    validate_body(resp.body());
}

void KeyKeeperAPI::unset(int kek_version)
{
    double key_keeper_timeout = timeout_;
    std::string key_keeper_uri = uri_;
    if (kek_version < 0)
        kek_version = 0;
    std::string target_id = get_target_id(kek_version);
    HttpParams params;
    params["id"] = target_id;
    HttpResponse resp = http_post(key_keeper_uri + "unset",
                                  logger_,
                                  key_keeper_timeout,
                                  "POST",
                                  get_headers(),
                                  params,
                                  "",
                                  ssl_validate_cert_);
    validate_status(resp.resp_code());
    validate_body(resp.body());
}

const std::string KeyKeeperAPI::get_target_id(int kek_version) const
{
    return "KEK_VER" + Yb::to_string(kek_version)
        + "_PART" + Yb::to_string(part_);
}

void KeyKeeperAPI::validate_status(int status) const
{
    if (status != 200)
        throw ::RunTimeError("KeyKeeperAPI: not HTTP 200");
}

void KeyKeeperAPI::validate_body(const std::string &body) const
{
    auto root = Yb::ElementTree::parse(body);
    if (root->find_first("status")->get_text() != "success")
        throw ::RunTimeError("KeyKeeperAPI: not success");
}


const ConfigMap
    TokenizerConfig::load_config_from_xml(IConfig &config)
{
    XmlConfig *xml_config = dynamic_cast<XmlConfig *> (&config);
    if (!xml_config)
        throw ::RunTimeError("can't access XML config");
    ConfigMap cf;
    Yb::ElementTree::ElementPtr items_node =
        xml_config->get_branch("KeySettings/items");
    auto item_nodes = items_node->find_children("item");
    auto i = item_nodes->begin(), iend = item_nodes->end();
    for (; i != iend; ++i) {
        auto &node = *i;
        const std::string &id = node->attrib_["id"];
        cf[id] = node->attrib_["data"];
    }
    return cf;
}

const ConfigMap
    TokenizerConfig::load_config_from_db(Yb::Session &session)
{
    ConfigMap cf;
    auto found_keys_rs = Yb::query<Domain::Config>(session)
        .filter_by(
                Domain::Config::c.ckey.like_(Yb::ConstExpr("KEK%")) ||
                Domain::Config::c.ckey.like_(Yb::ConstExpr("HMAC%")) ||
                Domain::Config::c.ckey == "STATE" ||
                Domain::Config::c.ckey == "STATE_EXTENSION")
        .all();
    auto i = found_keys_rs.begin(), iend = found_keys_rs.end();
    for (; i != iend; ++i) {
        auto &found_key = *i;
        cf[found_key.ckey] = found_key.cvalue;
    }
    return cf;
}

static const std::string decode_part(const std::string &s, int part_n)
{
    const std::string kek_n = string_from_hexstring(s, HEX_NOSPACES);
    if (kek_n.size() != 32)
        throw ::RunTimeError("invalid KEK" + Yb::to_string(part_n) +
                             " size: " + Yb::to_string(kek_n.size()));
    return kek_n;
}

void TokenizerConfig::assemble_master_keys(
        Yb::ILogger &logger,
        IConfig &config,
        const ConfigMap &xml_params,
        const ConfigMap &db_params,
        VersionMap &master_key_parts1,
        VersionMap &master_key_parts2,
        VersionMap &master_key_parts3,
        VersionMap &master_keys,
        CheckMap &valid_master_keys)
{
    VersionMap mk_parts1, mk_parts2, mk_parts3, mk;
    CheckMap mk_valid;
    auto kk_config = get_keykeeper_controller(config);
    KeyKeeperAPI kk_api(kk_config.get<0>(), kk_config.get<1>(),
                        kk_config.get<2>(), &logger,
                        theApp::instance().is_prod());
    auto secret=kk_config.get<3>();
    kk_api.set_secret(secret);
    const std::string prefix = "KEK_VER", suffix = "_PART3";
    auto i = db_params.begin(), iend = db_params.end();
    for (; i != iend; ++i) {
        const std::string &id = i->first;
        if (!starts_with(id, prefix) || !ends_with(id, suffix))
            continue;
        auto ver_str = id.substr(prefix.size(),
                id.size() - prefix.size() - suffix.size());
        try {
            logger.debug("assembling master key ver" + ver_str);
            const auto &kek3_hex = i->second;
            int ver = boost::lexical_cast<int>(ver_str);
            const auto &kek1_hex = kk_api.get_key_by_version(ver);
            auto j = xml_params.find(prefix + ver_str + "_PART2");
            if (xml_params.end() == j)
                throw ::RunTimeError("can't access KEK part2");
            const auto &kek2_hex = j->second;
            auto master_key = assemble_kek(kek1_hex, kek2_hex, kek3_hex);
            mk_parts1[ver] = kek1_hex;
            mk_parts2[ver] = kek2_hex;
            mk_parts3[ver] = kek3_hex;
            mk[ver] = master_key;
            logger.info("master key ver" + ver_str + " assembled OK");
            mk_valid[ver] = check_kek(logger,
                    ver, master_key, db_params);
        }
        catch (const std::exception &e) {
            logger.error("assembling master key ver"
                    + ver_str + " failed: "
                    + std::string(e.what()));
        }
    }
    std::swap(mk_parts1, master_key_parts1);
    std::swap(mk_parts2, master_key_parts2);
    std::swap(mk_parts3, master_key_parts3);
    std::swap(mk, master_keys);
    std::swap(mk_valid, valid_master_keys);
}

bool TokenizerConfig::check_kek(
        Yb::ILogger &logger,
        int kek_version,
        const std::string &master_key,
        const ConfigMap &db_params)
{
    AESCrypter aes_crypter(master_key);
    bool result = false;
    try {
        auto i = db_params.find("KEK_CONTROL_PHRASE");
        YB_ASSERT(db_params.end() != i);
        const auto &control_phrase = i->second;
        i = db_params.find(
                "KEK_VER" + Yb::to_string(kek_version)
                + "_CONTROL_CODE");
        YB_ASSERT(db_params.end() != i);
        const auto &control_code = i->second;
        auto encrypted_phrase = encode_base64(
                aes_crypter.encrypt(control_phrase));
        result = encrypted_phrase == control_code;
    }
    catch (const std::exception &e) {
        logger.error("failed to acquire required params: "
                + std::string(e.what()));
    }
    if (!result)
        logger.error("failed to validate KEK ver" +
                Yb::to_string(kek_version));
    else
        logger.info("valid KEK ver" + Yb::to_string(kek_version));
    return result;
}

const VersionMap
    TokenizerConfig::load_hmac_keys(
            Yb::ILogger &logger, const ConfigMap &db_params,
            Yb::Session &session, const VersionMap &master_keys)
{
    VersionMap hk;
    const std::string prefix = "HMAC_VER", suffix = "_ID";
    auto i = db_params.begin(), iend = db_params.end();
    for (; i != iend; ++i) {
        const std::string &id = i->first;
        if (!starts_with(id, prefix) || !ends_with(id, suffix))
            continue;
        auto ver_str = id.substr(prefix.size(),
                id.size() - prefix.size() - suffix.size());
        try {
            logger.debug("loading HMAC key ver" + ver_str);
            int ver = boost::lexical_cast<int>(ver_str);
            int hmac_id = boost::lexical_cast<int>(i->second);
            Domain::DataKey hmac_key = Yb::query<Domain::DataKey>(session)
                .filter_by(Domain::DataKey::c.id == hmac_id).one();
            int kek_version = hmac_key.kek_version;
            auto mk = master_keys.find(kek_version);
            if (master_keys.end() == mk)
                throw ::RunTimeError("can't decode HMAC using KEK ver" +
                                     Yb::to_string(kek_version));
            auto hmac = Tokenizer::decrypt_data(mk->second,
                                                hmac_key.dek_crypted);
            hk[ver] = hmac;
            logger.info("HMAC key ver" + ver_str + " loaded OK");
        }
        catch (const std::exception &e) {
            logger.error("loading HMAC key ver" + ver_str + " failed: "
                    + std::string(e.what()));
        }
    }
    return hk;
}

TokenizerConfig::TokenizerConfig(bool hmac_needed)
    : ts_(0)
{
    reload(hmac_needed);
}

const std::string TokenizerConfig::assemble_kek(
        const std::string &kek1_hex,
        const std::string &kek2_hex,
        const std::string &kek3_hex)
{
    const auto kek1 = decode_part(kek1_hex, 1);
    const auto kek2 = decode_part(kek2_hex, 2);
    const auto kek3 = decode_part(kek3_hex, 3);
    std::string kek = kek1;
    xor_buffer(kek, kek2);
    xor_buffer(kek, kek3);
    auto master_key = sha256_digest(kek);
    return master_key;
}

const std::string
    TokenizerConfig::get_xml_config_key(const std::string &config_key) const
{
    Yb::ScopedLock lock(mux_);
    auto i = xml_params_.find(config_key);
    if (xml_params_.end() == i)
        throw Yb::KeyError("xml key not found: " + config_key);
    return i->second;
}

const std::string
    TokenizerConfig::get_db_config_key(const std::string &config_key) const
{
    Yb::ScopedLock lock(mux_);
    auto i = db_params_.find(config_key);
    if (db_params_.end() == i)
        throw Yb::KeyError("db key not found: " + config_key);
    return i->second;
}

int TokenizerConfig::get_active_master_key_version() const
{
    return boost::lexical_cast<int>(get_db_config_key("KEK_VERSION"));
}

const std::string TokenizerConfig::get_master_key(int version, bool valid_only) const
{
    Yb::ScopedLock lock(mux_);
    auto i = master_keys_.find(version);
    if (master_keys_.end() == i)
        throw Yb::KeyError("master key not found: " + Yb::to_string(version));
    if (!valid_only || is_kek_valid(version))
        return i->second;
    throw Yb::KeyError("master key not valid: " + Yb::to_string(version));
}

const VersionMap TokenizerConfig::get_master_keys(bool valid_only) const
{
    Yb::ScopedLock lock(mux_);
    VersionMap result;
    auto i = master_keys_.begin(), iend = master_keys_.end();
    for (; i != iend; ++i) {
        if (!valid_only || is_kek_valid(i->first))
            result.insert(*i);
    }
    return result;
}

int TokenizerConfig::get_active_hmac_key_version() const
{
    return boost::lexical_cast<int>(get_db_config_key("HMAC_VERSION"));
}

const std::string TokenizerConfig::get_hmac_key(int version) const
{
    Yb::ScopedLock lock(mux_);
    auto i = hmac_keys_.find(version);
    if (hmac_keys_.end() == i)
        throw Yb::KeyError("hmac key not found: " + Yb::to_string(version));
    return i->second;
}

const VersionMap TokenizerConfig::get_hmac_keys() const
{
    Yb::ScopedLock lock(mux_);
    VersionMap result(hmac_keys_);
    return result;
}

const std::vector<int> TokenizerConfig::get_hmac_versions() const
{
    std::vector<int> versions;
    int active_hmac_ver = get_active_hmac_key_version();
    versions.push_back(active_hmac_ver);
    Yb::ScopedLock lock(mux_);
    auto i = hmac_keys_.begin(), iend = hmac_keys_.end();
    for (; i != iend; ++i)
        if (i->first != active_hmac_ver)
            versions.push_back(i->first);
    return versions;
}

void TokenizerConfig::reload(bool hmac_needed)
{
    Yb::ILogger::Ptr logger(
            theApp::instance().new_logger("tokenizer_config").release());
    if (hmac_needed)
        logger->info("reloading");
    else
        logger->info("reloading (no HMAC keys)");

    IConfig &config(theApp::instance().cfg());
    ConfigMap xml_params(load_config_from_xml(config));

    std::auto_ptr<Yb::Session> session(
            theApp::instance().new_session().release());
    ConfigMap db_params(load_config_from_db(*session));

    VersionMap master_key_parts1, master_key_parts2,
               master_key_parts3, master_keys;
    CheckMap valid_master_keys;
    assemble_master_keys(
            *logger, config, xml_params, db_params,
            master_key_parts1, master_key_parts2,
            master_key_parts3, master_keys,
            valid_master_keys);

    VersionMap hmac_keys;
    if (hmac_needed)
        hmac_keys = load_hmac_keys(
                *logger, db_params, *session, master_keys);
    session.reset(NULL);

    Yb::ScopedLock lock(mux_);
    std::swap(xml_params, xml_params_);
    std::swap(db_params, db_params_);
    std::swap(master_key_parts1, master_key_parts1_);
    std::swap(master_key_parts2, master_key_parts2_);
    std::swap(master_key_parts3, master_key_parts3_);
    std::swap(master_keys, master_keys_);
    std::swap(valid_master_keys, valid_master_keys_);
    std::swap(hmac_keys, hmac_keys_);
    ts_ = time(NULL);
}

TokenizerConfig &TokenizerConfig::refresh(bool force_refresh)
{
    time_t now = time(NULL);
    if (now - ts_ > 15 || force_refresh) {
        ts_ = now;
        IConfig &config(theApp::instance().cfg());
        config.reload();
        reload();
    }
    return *this;
}

const std::string &
    TokenizerConfig::get_master_key_component(int version, int part) const
{
    const VersionMap *key_parts = &master_key_parts1_;
    if (part == 2)
        key_parts = &master_key_parts2_;
    else if (part == 3)
        key_parts = &master_key_parts3_;
    auto i = key_parts->find(version);
    YB_ASSERT(key_parts->end() != i);
    return i->second;
}

int TokenizerConfig::get_current_version() const
{
    // deprecated
    return get_active_master_key_version();
}

const std::vector<int>
    TokenizerConfig::get_versions(bool include_incomplete) const
{
    // deprecated
    std::vector<int> result;
    for (auto i = valid_master_keys_.begin();
            i != valid_master_keys_.end(); ++i)
    {
        if (i->second || include_incomplete)
            result.push_back(i->first);
    }
    return result;
}

bool TokenizerConfig::is_kek_valid(int version) const
{
    auto i = valid_master_keys_.find(version);
    if (i != valid_master_keys_.end())
        return i->second;
    return false;
}

bool TokenizerConfig::is_kek_part_checked(int version, int part) const
{
    const std::string key = "KEK_VER"
        + Yb::to_string(version) + "_PART"
        + Yb::to_string(part) + "_CHECK";
    return get_db_config_key(key) == "1";
}

bool TokenizerConfig::is_version_checked(int version) const
{
    return is_kek_part_checked(version, 1)
        && is_kek_part_checked(version, 2)
        && is_kek_part_checked(version, 3);
}

int TokenizerConfig::get_last_version() const
{
    auto versions = get_versions();
    auto i = std::max_element(versions.begin(), versions.end());
    YB_ASSERT(versions.end() != i);
    return *i;
}

int TokenizerConfig::get_switch_version() const
{
    int version = get_active_master_key_version();
    try {
        const std::string key = "KEK_TARGET_VERSION";
        version = boost::lexical_cast<int>(get_db_config_key(key));
    }
    catch (const Yb::KeyError &) { }

    if (!is_version_checked(version))
        throw ::RunTimeError("KEK version not confirmed: " +
                Yb::to_string(version));
    return version;
}


Tokenizer::Tokenizer(IConfig &config, Yb::ILogger &logger,
                     Yb::Session &session, bool card_tokenizer)
    : card_tokenizer_(card_tokenizer)
    , config_(config)
    , logger_(logger.new_logger("tokenizer").release())
    , session_(session)
#ifdef TOKENIZER_CONFIG_SINGLETON
    , tokenizer_config_(theTokenizerConfig::instance().refresh())
#endif
{}

const std::string Tokenizer::search(const std::string &plain_text)
{
    std::string result;
    const std::vector<int> hmac_versions
        = tokenizer_config().get_hmac_versions();
    auto i = hmac_versions.begin(), iend = hmac_versions.end();
    for (; i != iend; ++i) {
        auto hmac_version = *i;
        auto hmac_digest = count_hmac(plain_text, hmac_version);
        try {
            if (card_tokenizer_)
                result = do_search<Domain::DataToken>(hmac_digest);
            else
                result = do_search<Domain::SecureVault>(hmac_digest);
            logger_->debug("deduplicated using HMAC ver"
                           + Yb::to_string(hmac_version));
            break;
        }
        catch (const Yb::NoDataFound &) {
            // TODO: log?
        }
    }
    return result;
}

const std::string Tokenizer::tokenize(const std::string &plain_text,
                                      const Yb::DateTime &finish_ts,
                                      bool deduplicate)
{
    std::string result;
    if (deduplicate) {
        result = search(plain_text);
        if (!result.empty())
            return result;
    }
    int hmac_version = tokenizer_config().get_active_hmac_key_version();
    std::string hmac_digest;
    if (plain_text.size() >= 10) {
        hmac_digest = count_hmac(plain_text, hmac_version);
    }
    else {
        hmac_digest =
            encode_base64(sha256_digest(generate_random_string(10)));
    }
    std::string encoded_text = encode_data(plain_text);
    std::string crypted;
    Domain::DataKey data_key = use_dek(encoded_text, crypted);
    std::string token_string = generate_token_string();
    if (card_tokenizer_)
        do_tokenize<Domain::DataToken>(
                finish_ts, hmac_version, hmac_digest,
                token_string, crypted, data_key);
    else
        do_tokenize<Domain::SecureVault>(
                finish_ts, hmac_version, hmac_digest,
                token_string, crypted, data_key);
    return token_string;
}

const std::string Tokenizer::detokenize(const std::string &token_string)
{
    std::string dek_crypted, data_crypted;
    int kek_version;
    try {
        if (card_tokenizer_)
            do_detokenize<Domain::DataToken>(token_string,
                    dek_crypted, kek_version, data_crypted);
        else
            do_detokenize<Domain::SecureVault>(token_string,
                    dek_crypted, kek_version, data_crypted);
    }
    catch (const Yb::NoDataFound &) {
        throw TokenNotFound();
    }
    tokenizer_config(false);
    std::string dek = decrypt_dek(dek_crypted, kek_version);
    return decode_data(decrypt_data(dek, data_crypted, card_tokenizer_));
}

bool Tokenizer::remove_data_token(const std::string &token_string)
{
    try {
        if (card_tokenizer_)
            do_delete<Domain::DataToken>(token_string);
        else
            do_delete<Domain::SecureVault>(token_string);
    }
    catch (const Yb::NoDataFound &) {
        return false;
    }
    return true;
}

const std::string Tokenizer::generate_token_string()
{
    while (true) {
        const std::string token_string = string_to_hexstring(
                generate_random_bytes(16), HEX_LOWERCASE | HEX_NOSPACES);
        int count = 0;
        if (card_tokenizer_)
            count = do_check_token<Domain::DataToken>(token_string);
        else
            count = do_check_token<Domain::SecureVault>(token_string);
        if (!count)  // TODO: too optimistic
            return token_string;
    }
}

const std::string Tokenizer::encode_data(const std::string &s)
{
    if (card_tokenizer_)
        return bcd_encode(s);
    return pkcs7_encode(s);
}

const std::string Tokenizer::decode_data(const std::string &s)
{
    if (card_tokenizer_)
        return bcd_decode(s);
    return pkcs7_decode(s);
}

const std::string Tokenizer::encrypt_data(const std::string &dek,
                                          const std::string &data,
                                          bool card_tokenizer)
{
    AESCrypter data_encrypter(dek, card_tokenizer? AES_CRYPTER_ECB: AES_CRYPTER_CBC);
    return encode_base64(data_encrypter.encrypt(data));
}

const std::string Tokenizer::decrypt_data(const std::string &dek,
                                          const std::string &data_crypted,
                                          bool card_tokenizer)
{
    AESCrypter data_encrypter(dek, card_tokenizer? AES_CRYPTER_ECB: AES_CRYPTER_CBC);
    return data_encrypter.decrypt(decode_base64(data_crypted));
}

const std::string Tokenizer::encrypt_dek(const std::string &dek,
                                        int kek_version)
{
    AESCrypter data_encrypter(tokenizer_config().get_master_key(kek_version),
                              AES_CRYPTER_ECB);
    return encode_base64(data_encrypter.encrypt(dek));
}

const std::string Tokenizer::decrypt_dek(const std::string &dek_crypted,
                                        int kek_version)
{
    AESCrypter data_encrypter(tokenizer_config().get_master_key(kek_version),
                              AES_CRYPTER_ECB);
    return data_encrypter.decrypt(decode_base64(dek_crypted));
}

TokenizerConfig &Tokenizer::tokenizer_config(bool hmac_needed)
{
#ifdef TOKENIZER_CONFIG_SINGLETON
    return tokenizer_config_;
#else
    if (tokenizer_config_.get())
        return *tokenizer_config_;
    tokenizer_config_.reset(new TokenizerConfig(hmac_needed));
    return *tokenizer_config_;
#endif
}

DEKPool &Tokenizer::dek_pool()
{
    if (dek_pool_.get())
        return *dek_pool_;
    TokenizerConfig &cfg = tokenizer_config();
    int kek_version = cfg.get_active_master_key_version();
    dek_pool_.reset(new DEKPool(config_, *logger_, session_,
                                cfg.get_master_key(kek_version),
                                kek_version));
    return *dek_pool_;
}

Domain::DataKey Tokenizer::use_dek(
        const std::string &plain_text, std::string &out)
{
    Domain::DataKey data_key = dek_pool().get_active_data_key();
    std::string dek = decrypt_dek(data_key.dek_crypted, data_key.kek_version);
    std::string crypted = encrypt_data(dek, plain_text, card_tokenizer_);
    std::swap(crypted, out);
    data_key.counter = data_key.counter + 1;
    if (data_key.counter >= data_key.max_counter)
        data_key.finish_ts = Yb::now();
    return data_key;
}

const std::string Tokenizer::count_hmac(const std::string &plain_text,
                                        const std::string &hmac_key)
{
    return encode_base64(hmac_sha256_digest(hmac_key, plain_text));
}

const std::string Tokenizer::count_hmac(const std::string &plain_text,
                                        int hmac_version)
{
    std::string hk = tokenizer_config().get_hmac_key(hmac_version);
    return count_hmac(plain_text, hk);
}

// vim:ts=4:sts=4:sw=4:et:
