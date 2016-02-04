// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__TOKENIZER_H
#define CARD_PROXY__TOKENIZER_H

#include <string>
#include <util/data_types.h>
#include <util/singleton.h>
#include <orm/data_object.h>

#include "utils.h"
#include "conf_reader.h"
#include "dek_pool.h"

#include "domain/DataToken.h"

typedef std::map<std::string, std::string> ConfigMap;
typedef std::map<int, std::string> VersionMap;


class KeyKeeperAPI
{
    ConfigMap cached_;
    IConfig *config_;
    Yb::ILogger *logger_;
    double timeout_;
    std::string uri_;
public:
    explicit KeyKeeperAPI(IConfig &config, Yb::ILogger *logger = NULL)
        : config_(&config), logger_(logger), timeout_(0)
    {}
    KeyKeeperAPI(const std::string &uri, double timeout,
                 Yb::ILogger *logger = NULL)
        : config_(NULL), logger_(logger), timeout_(timeout), uri_(uri)
    {}
    const std::string recv_key_from_server_v1(int kek_version);
    const std::string recv_key_from_server_v2(int kek_version);
    const std::string recv_key_from_server(int kek_version);
    const std::string get_key_by_version(int kek_version);
    void send_key_to_server_v1(const std::string &key, int kek_version);
    void send_key_to_server_v2(const std::string &key, int kek_version);
    void send_key_to_server(const std::string &key, int kek_version);
};


class TokenizerConfig
{
    ConfigMap xml_params_;
    ConfigMap db_params_;
    VersionMap master_keys_;
    VersionMap hmac_keys_;

    mutable Yb::Mutex mux_;
    time_t ts_;

    static const ConfigMap load_config_from_xml(IConfig &config);
    static const ConfigMap load_config_from_db(Yb::Session &session);

    static const VersionMap assemble_master_keys(
            Yb::ILogger &logger, IConfig &config,
            const ConfigMap &xml_params, const ConfigMap &db_params);

    static const VersionMap load_hmac_keys(
            Yb::ILogger &logger, const ConfigMap &db_params,
            Yb::Session &session, const VersionMap &master_keys);

    // non-copyable
    TokenizerConfig(const TokenizerConfig &);
    TokenizerConfig &operator=(const TokenizerConfig &);
public:
    TokenizerConfig();

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
    void reload();
    TokenizerConfig &refresh();
};

typedef Yb::SingletonHolder<TokenizerConfig> theTokenizerConfig;


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
    Yb::ILogger::Ptr logger_;
    Yb::Session &session_;
    TokenizerConfig &tokenizer_config_;
    DEKPool dek_pool_;

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
