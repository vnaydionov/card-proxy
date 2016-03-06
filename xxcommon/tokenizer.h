// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__TOKENIZER_H
#define CARD_PROXY__TOKENIZER_H

#include <string>
#include <algorithm>
#include <boost/tuple/tuple.hpp>
#include <util/data_types.h>
#include <util/singleton.h>
#include <orm/data_object.h>

#include "utils.h"
#include "conf_reader.h"
#include "dek_pool.h"

#include "domain/DataToken.h"

#define TOKENIZER_CONFIG_SINGLETON

typedef std::map<std::string, std::string> ConfigMap;
typedef std::map<int, std::string> VersionMap;
typedef std::map<int, bool> CheckMap;


boost::tuple<std::string, double, int> get_keykeeper_controller(
        IConfig &config);

class KeyKeeperAPI
{
public:
    KeyKeeperAPI(const std::string &uri, double timeout, int part,
                 Yb::ILogger *logger = NULL, bool ssl_validate_cert = true)
        : logger_(logger), uri_(uri), timeout_(timeout), part_(part)
        , ssl_validate_cert_(ssl_validate_cert)
    {
        YB_ASSERT(!uri_.empty());
        YB_ASSERT(part_ == 1 || part_ == 2);
    }
    const std::string recv_key_from_server(int kek_version);
    const std::string &get_key_by_version(int kek_version);
    void send_key_to_server(const std::string &key, int kek_version);
    void cleanup(int kek_version);
    void unset(int kek_version);

private:
    ConfigMap cached_;
    Yb::ILogger *logger_;
    std::string uri_;
    double timeout_;
    int part_;
    bool ssl_validate_cert_;

    const std::string get_target_id(int kek_version) const;
    void validate_status(int status) const;
    void validate_body(const std::string &body) const;
};


class TokenizerConfig
{
    ConfigMap xml_params_;
    ConfigMap db_params_;
    VersionMap master_key_parts1_;
    VersionMap master_key_parts2_;
    VersionMap master_key_parts3_;
    VersionMap master_keys_;
    CheckMap valid_master_keys_;
    VersionMap hmac_keys_;

    mutable Yb::Mutex mux_;
    time_t ts_;

    static const ConfigMap load_config_from_xml(IConfig &config);

    static const ConfigMap load_config_from_db(
            Yb::Session &session);

    static void assemble_master_keys(
            Yb::ILogger &logger,
            IConfig &config,
            const ConfigMap &xml_params,
            const ConfigMap &db_params,
            VersionMap &master_key_parts1,
            VersionMap &master_key_parts2,
            VersionMap &master_key_parts3,
            VersionMap &master_keys,
            CheckMap &valid_master_keys);

    static bool check_kek(
            Yb::ILogger &logger,
            int kek_version,
            const std::string &master_key,
            const ConfigMap &db_params);

    static const VersionMap load_hmac_keys(
            Yb::ILogger &logger, const ConfigMap &db_params,
            Yb::Session &session, const VersionMap &master_keys);

    // non-copyable
    TokenizerConfig(const TokenizerConfig &);
    TokenizerConfig &operator=(const TokenizerConfig &);
public:
    TokenizerConfig(bool hmac_needed = true);

    static const std::string assemble_kek(
        const std::string &kek1_hex,
        const std::string &kek2_hex,
        const std::string &kek3_hex);

    const std::string get_xml_config_key(const std::string &config_key) const;
    const std::string get_db_config_key(const std::string &config_key) const;

    int get_active_master_key_version() const;
    const std::string get_master_key(int version) const;
    const std::string get_active_master_key() const {
        return get_master_key(get_active_master_key_version());
    }
    const VersionMap get_master_keys() const;

    int get_active_hmac_key_version() const;
    const std::string get_hmac_key(int version) const;
    const std::string get_active_hmac_key() const {
        return get_hmac_key(get_active_hmac_key_version());
    }
    const VersionMap get_hmac_keys() const;
    const std::vector<int> get_hmac_versions() const;

    time_t get_ts() const { return ts_; }
    void reload(bool hmac_needed = true);
    TokenizerConfig &refresh(bool force_refresh = false);

    const std::string &get_master_key_component(
            int version, int part) const;
    int get_current_version() const;
    const std::vector<int> get_versions(bool include_incomplete = true) const;
    bool is_kek_valid(int version) const;
    bool is_kek_part_checked(int version, int part) const;
    bool is_version_checked(int version) const;
    int get_last_version() const;
    int get_switch_version() const;

};

#ifdef TOKENIZER_CONFIG_SINGLETON
typedef Yb::SingletonHolder<TokenizerConfig> theTokenizerConfig;
#endif


class Tokenizer
{
public:
    Tokenizer(IConfig &config, Yb::ILogger &logger, Yb::Session &session);

    const std::string search(const std::string &plain_text);
    const std::string tokenize(const std::string &plain_text,
                               const Yb::DateTime &finish_ts,
                               bool deduplicate = true);
    const std::string detokenize(const std::string &token_string);
    bool remove_data_token(const std::string &token_string);

    const std::string generate_token_string();

    static const std::string encode_data(const std::string &dek,
                                         const std::string &data);
    static const std::string decode_data(const std::string &dek,
                                         const std::string &data_crypted);

    const std::string encode_dek(const std::string &dek,
                                 int kek_version);
    const std::string decode_dek(const std::string &dek_crypted,
                                 int kek_version);

private:
    IConfig &config_;
    Yb::ILogger::Ptr logger_;
    Yb::Session &session_;
#ifdef TOKENIZER_CONFIG_SINGLETON
    TokenizerConfig &tokenizer_config_;
#else
    std::auto_ptr<TokenizerConfig> tokenizer_config_;
#endif
    std::auto_ptr<DEKPool> dek_pool_;

    TokenizerConfig &tokenizer_config(bool hmac_needed = true);
    DEKPool &dek_pool();

    Domain::DataToken do_tokenize(const std::string &plain_text,
                                  const Yb::DateTime &finish_ts,
                                  int hmac_version,
                                  const std::string &hmac_digest);
    const std::string count_hmac(const std::string &plain_text,
                                 int hmac_version);
};


class TokenNotFound: public RunTimeError
{
public:
    TokenNotFound(): RunTimeError("Token not found") {}
};

#endif // CARD_PROXY__TOKENIZER_H
// vim:ts=4:sts=4:sw=4:et:
