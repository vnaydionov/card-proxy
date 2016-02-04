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
    KeyKeeperAPI::recv_key_from_server_v1(int kek_version)
{
    YB_ASSERT(config_ != NULL);
    // TODO: support versioning
    TcpSocket sock(-1, config_->get_value_as_int("KeyKeeper/Timeout"));
    sock.connect(config_->get_value("KeyKeeper/Host"),
                 config_->get_value_as_int("KeyKeeper/Port"));
    sock.write("get\n");
    std::string response = sock.readline();
    sock.close(true);
    std::string prefix = "key=";
    if (response.substr(0, 4) != prefix)
        throw RunTimeError("KeyKeeper protocol error");
    size_t pos = response.find(' ');
    if (std::string::npos == pos)
        throw RunTimeError("KeyKeeper protocol error");
    return response.substr(prefix.size(), pos - prefix.size());
}

const std::string
    KeyKeeperAPI::recv_key_from_server_v2(int kek_version)
{
    double key_keeper_timeout;
    std::string key_keeper_uri;
    if (config_) {
        key_keeper_timeout =
            config_->get_value_as_int("KeyKeeper2/Timeout")/1000.;
        key_keeper_uri = config_->get_value("KeyKeeper2/URL");
    }
    else {
        key_keeper_timeout = timeout_;
        key_keeper_uri = uri_;
    }
    std::string target_id =
        "KEK_VER" + Yb::to_string(kek_version) + "_PART1";
    std::string body = http_post(key_keeper_uri + "get",
                                 HttpParams(), key_keeper_timeout, "GET",
                                 logger_);
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
    KeyKeeperAPI::recv_key_from_server(int kek_version)
{
    if ((config_ && config_->has_key("KeyKeeper2/Timeout"))
            || !uri_.empty())
        return recv_key_from_server_v2(kek_version);
    return recv_key_from_server_v1(kek_version);
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

void KeyKeeperAPI::send_key_to_server_v1(const std::string &key,
                                         int kek_version)
{
    YB_ASSERT(config_ != NULL);
    // TODO: support versioning
    TcpSocket sock(-1, config_->get_value_as_int("KeyKeeper/Timeout"));
    sock.connect(config_->get_value("KeyKeeper/Host"),
                 config_->get_value_as_int("KeyKeeper/Port"));
    sock.write("set key=" + key + "\n");
    sock.close(true);
}

void KeyKeeperAPI::send_key_to_server_v2(const std::string &key,
                                         int kek_version)
{
    double key_keeper_timeout;
    std::string key_keeper_uri;
    if (config_) {
        key_keeper_timeout =
            config_->get_value_as_int("KeyKeeper2/Timeout")/1000.;
        key_keeper_uri = config_->get_value("KeyKeeper2/URL");
    }
    else {
        key_keeper_timeout = timeout_;
        key_keeper_uri = uri_;
    }
    if (kek_version < 0)
        kek_version = 0;
    std::string target_id = "KEK_VER" + Yb::to_string(kek_version) + "_PART1";
    HttpParams params;
    params["id"] = target_id;
    params["data"] = key;
    std::string body = http_post(key_keeper_uri + "set",
                                 params, key_keeper_timeout, "POST",
                                 logger_);
    auto root = Yb::ElementTree::parse(body);
    if (root->find_first("status")->get_text() != "success")
        throw ::RunTimeError("send_key_to_server: not success");
}

void KeyKeeperAPI::send_key_to_server(const std::string &key,
                                      int kek_version)
{
    if ((config_ && config_->has_key("KeyKeeper2/Timeout"))
            || !uri_.empty())
        return send_key_to_server_v2(key, kek_version);
    return send_key_to_server_v1(key, kek_version);
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
                Domain::Config::c.ckey.like_(Yb::ConstExpr("HMAC%")))
        .all();
    auto i = found_keys_rs.begin(), iend = found_keys_rs.end();
    for (; i != iend; ++i) {
        auto &found_key = *i;
        cf[found_key.ckey] = found_key.cvalue;
    }
    return cf;
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
            const std::string kek3 = string_from_hexstring(
                    i->second, HEX_NOSPACES);
            if (kek3.size() != 32)
                throw ::RunTimeError("invalid KEK3 size: " +
                        Yb::to_string(kek3.size()));
            int ver = boost::lexical_cast<int>(ver_str);
            const std::string kek1 = string_from_hexstring(
                    kk_api.get_key_by_version(ver), HEX_NOSPACES);
            if (kek1.size() != 32)
                throw ::RunTimeError("invalid KEK1 size: " +
                        Yb::to_string(kek1.size()));
            ConfigMap::const_iterator j = xml_params.find(
                    prefix + ver_str + "_PART2");
            if (xml_params.end() == j)
                throw ::RunTimeError("can't access KEK part2");
            const std::string kek2 = string_from_hexstring(
                    j->second, HEX_NOSPACES);
            if (kek2.size() != 32)
                throw ::RunTimeError("invalid KEK2 size: " +
                        Yb::to_string(kek2.size()));
            std::string kek(32, ' ');
            for (int k = 0; k < 32; ++k)
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
            auto mk = master_keys.find(hmac_key.kek_version);
            if (master_keys.end() == mk)
                throw ::RunTimeError("can't decode HMAC using KEK ver" +
                                     Yb::to_string(kek_version));
            auto hmac = Tokenizer::decode_data(mk->second,
                                               hmac_key.dek_crypted);
            hk[ver] = hmac;
            logger.info("HMAC key ver" + ver_str + " assembled OK");
        }
        catch (const std::exception &e) {
            logger.error("loading HMAC key ver" + ver_str + " failed: "
                    + std::string(e.what()));
        }
    }
    return hk;
}

TokenizerConfig::TokenizerConfig()
    : ts_(0)
{
    reload();
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

void TokenizerConfig::reload()
{
    Yb::ILogger::Ptr logger(
            theApp::instance().new_logger("tokenizer_config").release());
    logger->info("reloading");

    IConfig &config(theApp::instance().cfg());
    config.reload();
    ConfigMap xml_params(load_config_from_xml(config));

    std::auto_ptr<Yb::Session> session(
            theApp::instance().new_session().release());
    ConfigMap db_params(load_config_from_db(*session));

    VersionMap master_keys(assemble_master_keys(
            *logger, config, xml_params, db_params));

    VersionMap hmac_keys(load_hmac_keys(
            *logger, db_params, *session, master_keys));
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
        reload();
    }
    return *this;
}


Tokenizer::Tokenizer(IConfig &config, Yb::ILogger &logger,
                     Yb::Session &session)
    : logger_(logger.new_logger("tokenizer").release())
    , session_(session)
    , tokenizer_config_(theTokenizerConfig::instance().refresh())
    , dek_pool_(config, *logger_, session,
                tokenizer_config_.get_active_master_key(),
                tokenizer_config_.get_active_master_key_version())
{}

const std::string Tokenizer::search(const std::string &plain_text)
{
    std::string result;
    const std::vector<int> hmac_versions
        = tokenizer_config_.get_hmac_versions();
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
    int hmac_version = tokenizer_config_.get_active_hmac_key_version();
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
            tokenizer_config_.get_master_key(kek_version),
            dek);
}

const std::string Tokenizer::decode_dek(const std::string &dek_crypted,
                                        int kek_version)
{
    return decode_data(
            tokenizer_config_.get_master_key(kek_version),
            dek_crypted);
}

Domain::DataToken Tokenizer::do_tokenize(const std::string &plain_text,
                                         const Yb::DateTime &finish_ts,
                                         int hmac_version,
                                         const std::string &hmac_digest)
{
    Domain::DataToken data_token;
    data_token.finish_ts = finish_ts;
    data_token.token_string = generate_token_string();
    Domain::DataKey data_key = dek_pool_.get_active_data_key();
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
    std::string hk = tokenizer_config_.get_hmac_key(hmac_version);
    return encode_base64(sha256_digest(hk + ":" + plain_text));
}

// vim:ts=4:sts=4:sw=4:et:
