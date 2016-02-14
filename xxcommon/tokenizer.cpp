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

using Yb::StrUtils::starts_with;
using Yb::StrUtils::ends_with;


const std::string
    KeyKeeperAPI::recv_key_from_server(int kek_version)
{
    double key_keeper_timeout = timeout_;
    std::string key_keeper_uri = uri_;
    if (config_) {
        key_keeper_timeout =
            config_->get_value_as_int("KeyKeeper2/Timeout")/1000.;
        key_keeper_uri = config_->get_value("KeyKeeper2/URL");
    }
    std::string target_id =
        "KEK_VER" + Yb::to_string(kek_version) + "_PART1";
    HttpResponse resp = http_post(key_keeper_uri + "get",
                                  logger_,
                                  key_keeper_timeout,
                                  "GET");
    if (resp.get<0>() != 200)
        throw ::RunTimeError("recv_key_from_server: not HTTP 200");
    const std::string &body = resp.get<2>();
    auto root = Yb::ElementTree::parse(body);
    if (root->find_first("status")->get_text() != "success")
        throw ::RunTimeError("recv_key_from_server: not success");
    std::string result;
    bool found = false;
    auto items_node = root->find_first("items");
    auto item_nodes = items_node->find_children("item");
    auto i = item_nodes->begin(), iend = item_nodes->end();
    for (; i != iend; ++i) {
        auto &node = *i;
        const std::string &id = node->attrib_["id"];
        cached_[id] = node->attrib_["data"];
        if (!starts_with(id, "KEK_VER") || !ends_with(id, "_PART1"))
            continue;
        if (kek_version < 0 || id == target_id) {
            result = node->attrib_["data"];
            found = true;
        }
    }
    if (!found)
        throw ::RunTimeError("recv_key_from_server: key not found");
    return result;
}

const std::string
    KeyKeeperAPI::get_key_by_version(int kek_version)
{
    std::string target_id =
        "KEK_VER" + Yb::to_string(kek_version) + "_PART1";
    auto i = cached_.find(target_id);
    if (cached_.end() == i)
        return recv_key_from_server(kek_version);
    return i->second;
}

void KeyKeeperAPI::send_key_to_server(const std::string &key,
                                      int kek_version)
{
    double key_keeper_timeout = timeout_;
    std::string key_keeper_uri = uri_;
    if (config_) {
        key_keeper_timeout =
            config_->get_value_as_int("KeyKeeper2/Timeout")/1000.;
        key_keeper_uri = config_->get_value("KeyKeeper2/URL");
    }
    if (kek_version < 0)
        kek_version = 0;
    std::string target_id = "KEK_VER" + Yb::to_string(kek_version) + "_PART1";
    HttpParams params;
    params["id"] = target_id;
    params["data"] = key;
    HttpResponse resp = http_post(key_keeper_uri + "set",
                                  logger_,
                                  key_keeper_timeout,
                                  "POST",
                                  HttpHeaders(),
                                  params);
    if (resp.get<0>() != 200)
        throw ::RunTimeError("recv_key_from_server: not HTTP 200");
    const std::string &body = resp.get<2>();
    auto root = Yb::ElementTree::parse(body);
    if (root->find_first("status")->get_text() != "success")
        throw ::RunTimeError("send_key_to_server: not success");
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
                Domain::Config::c.ckey == "STATUS")
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

const VersionMap
    TokenizerConfig::assemble_master_keys(
            Yb::ILogger &logger, IConfig &config,
            const ConfigMap &xml_params, const ConfigMap &db_params)
{
    VersionMap mk;
    KeyKeeperAPI kk_api(config, &logger);
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
            const std::string kek3 = decode_part(i->second, 3);
            int ver = boost::lexical_cast<int>(ver_str);
            const std::string kek1 = decode_part(
                    kk_api.get_key_by_version(ver), 1);
            ConfigMap::const_iterator j = xml_params.find(
                    prefix + ver_str + "_PART2");
            if (xml_params.end() == j)
                throw ::RunTimeError("can't access KEK part2");
            const std::string kek2 = decode_part(j->second, 2);
            std::string kek(kek1.size(), ' ');
            for (size_t k = 0; k < kek1.size(); ++k)
                kek[k] = kek1[k] ^ kek2[k] ^ kek3[k];
            mk[ver] = sha256_digest(kek);
            logger.info("master key ver" + ver_str + " assembled OK");
        }
        catch (const std::exception &e) {
            logger.error("assembling master key ver" + ver_str + " failed: "
                    + std::string(e.what()));
        }
    }
    return mk;
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
            auto hmac = Tokenizer::decode_data(mk->second,
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

const std::string TokenizerConfig::get_master_key(int version) const
{
    Yb::ScopedLock lock(mux_);
    auto i = master_keys_.find(version);
    if (master_keys_.end() == i)
        throw Yb::KeyError("master key not found: " + Yb::to_string(version));
    return i->second;
}

const VersionMap TokenizerConfig::get_master_keys() const
{
    Yb::ScopedLock lock(mux_);
    VersionMap result(master_keys_);
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

    VersionMap master_keys(assemble_master_keys(
            *logger, config, xml_params, db_params));

    VersionMap hmac_keys;
    if (hmac_needed)
        hmac_keys = load_hmac_keys(
                *logger, db_params, *session, master_keys);
    session.reset(NULL);

    Yb::ScopedLock lock(mux_);
    std::swap(xml_params, xml_params_);
    std::swap(db_params, db_params_);
    std::swap(master_keys, master_keys_);
    std::swap(hmac_keys, hmac_keys_);
    ts_ = time(NULL);
}

TokenizerConfig &TokenizerConfig::refresh()
{
    time_t now = time(NULL);
    if (now - ts_ > 15) {
        ts_ = now;
        IConfig &config(theApp::instance().cfg());
        config.reload();
        reload();
    }
    return *this;
}


Tokenizer::Tokenizer(IConfig &config, Yb::ILogger &logger,
                     Yb::Session &session)
    : config_(config)
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
            Domain::DataToken data_token =
                Yb::query<Domain::DataToken>(session_)
                .select_from<Domain::DataToken>()
                .join<Domain::DataKey>()
                .filter_by(Domain::DataToken::c.hmac_digest == hmac_digest)
                .one();
            result = data_token.token_string;
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
    if (deduplicate) {
        hmac_digest = count_hmac(plain_text, hmac_version);
    }
    else {
        hmac_digest =
            encode_base64(sha256_digest(generate_random_string(10)));
    }
    Domain::DataToken data_token = do_tokenize(plain_text, finish_ts,
                                               hmac_version, hmac_digest);
    return data_token.token_string;
}

const std::string Tokenizer::detokenize(const std::string &token_string)
{
    Domain::DataToken data_token;
    try {
        data_token = Yb::query<Domain::DataToken>(session_)
            .select_from<Domain::DataToken>()
            .join<Domain::DataKey>()
            .filter_by(Domain::DataToken::c.token_string == token_string)
            .one();
    }
    catch (const Yb::NoDataFound &) {
        throw TokenNotFound();
    }
    tokenizer_config(false);
    std::string dek = decode_dek(data_token.dek->dek_crypted,
                                 data_token.dek->kek_version);
    return bcd_decode(decode_data(dek, data_token.data_crypted));
}

bool Tokenizer::remove_data_token(const std::string &token_string)
{
    try {
        Domain::DataToken data_token =
            Yb::query<Domain::DataToken>(session_)
                .filter_by(Domain::DataToken::c.token_string == token_string)
                .one();
        data_token.delete_object();
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
        int count = Yb::query<Domain::DataToken>(session_)
                .filter_by(Domain::DataToken::c.token_string == token_string)
                .count();
        if (!count)  // TODO: too optimistic
            return token_string;
    }
}

const std::string Tokenizer::encode_data(const std::string &dek,
                                         const std::string &data)
{
    AESCrypter data_encrypter(dek);
    return encode_base64(data_encrypter.encrypt(data));
}

const std::string Tokenizer::decode_data(const std::string &dek,
                                         const std::string &data_crypted)
{
    AESCrypter data_encrypter(dek);
    return data_encrypter.decrypt(decode_base64(data_crypted));
}

const std::string Tokenizer::encode_dek(const std::string &dek,
                                        int kek_version)
{
    return encode_data(
            tokenizer_config().get_master_key(kek_version),
            dek);
}

const std::string Tokenizer::decode_dek(const std::string &dek_crypted,
                                        int kek_version)
{
    return decode_data(
            tokenizer_config().get_master_key(kek_version),
            dek_crypted);
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

Domain::DataToken Tokenizer::do_tokenize(const std::string &plain_text,
                                         const Yb::DateTime &finish_ts,
                                         int hmac_version,
                                         const std::string &hmac_digest)
{
    Domain::DataToken data_token;
    data_token.finish_ts = finish_ts;
    data_token.token_string = generate_token_string();
    Domain::DataKey data_key = dek_pool().get_active_data_key();
    std::string dek = decode_dek(data_key.dek_crypted, data_key.kek_version);
    data_token.data_crypted = encode_data(dek, bcd_encode(plain_text));
    data_token.dek = Domain::DataKey::Holder(data_key);
    data_token.hmac_version = hmac_version;
    data_token.hmac_digest = hmac_digest;
    data_token.save(session_);
    data_key.counter = data_key.counter + 1;
    if (data_key.counter >= data_key.max_counter)
        data_key.finish_ts = Yb::now();
    return data_token;
}

const std::string Tokenizer::count_hmac(const std::string &plain_text,
                                        int hmac_version)
{
    std::string hk = tokenizer_config().get_hmac_key(hmac_version);
    return encode_base64(sha256_digest(hk + ":" + plain_text));
}

// vim:ts=4:sts=4:sw=4:et:
