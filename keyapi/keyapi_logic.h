// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__KEY_API_LOGIC_H
#define CARD_PROXY__KEY_API_LOGIC_H

#include <string>
#include <map>
#include <stdexcept>
#include <util/nlogger.h>
#include <util/element_tree.h>
#include <orm/data_object.h>
#include "conf_reader.h"
#include "http_post.h"
#include "tokenizer.h"

class KeyAPI: private Yb::NonCopyable
{
    Yb::Logger::Ptr log_;
    Yb::Session &session_;

    static double get_time();
    static const std::string format_ts(double ts);

public:
    KeyAPI(IConfig &cfg, Yb::ILogger &log, Yb::Session &session);

    const std::string get_config_param(const std::string &key);
    void set_config_param(const std::string &key, const std::string &value);
    void unset_config_param(const std::string &key);
    const std::string get_state();
    void set_state(const std::string &state);

    std::pair<int, int> kek_auth(TokenizerConfig &cfg, const std::string &mode,
            const std::string &password, int kek_version = -1);

    Yb::ElementTree::ElementPtr mk_resp(
            const std::string &status = "success");
    Yb::ElementTree::ElementPtr generate_hmac(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr generate_kek(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr get_component(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr confirm_component(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr reset_target_version(const Yb::StringDict &params);

    void cleanup_kek(int kek_version);
    Yb::ElementTree::ElementPtr cleanup(const Yb::StringDict &params);

    void rehash_token(TokenizerConfig &tcfg, Domain::DataToken &data_token,
                      Domain::DataKey &data_key, int target_hmac_version);
    Yb::ElementTree::ElementPtr rehash_tokens(const Yb::StringDict &params);

    void reencrypt_dek(TokenizerConfig &tcfg, Domain::DataKey &data_key,
                       int target_kek_version);
    Yb::ElementTree::ElementPtr reencrypt_deks(const Yb::StringDict &params);

    Yb::ElementTree::ElementPtr switch_hmac(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr switch_kek(const Yb::StringDict &params);

    const VersionMap get_kek_use_counts();
    const VersionMap get_hmac_use_counts();
    Yb::ElementTree::ElementPtr status(const Yb::StringDict &params);
};

#endif // CARD_PROXY__KEY_API_LOGIC_H
// vim:ts=4:sts=4:sw=4:et:
