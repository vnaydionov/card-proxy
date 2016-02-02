// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__KEY_KEEPER_LOGIC_H
#define CARD_PROXY__KEY_KEEPER_LOGIC_H

#include <string>
#include <map>
#include <stdexcept>
#include <util/nlogger.h>
#include <util/element_tree.h>
#include "conf_reader.h"
#include "http_post.h"

Yb::LongInt _get_random();
double get_time();

class KeyKeeper
{
    KeyKeeper(const KeyKeeper &);
    KeyKeeper &operator=(const KeyKeeper &);

    std::string app_key_, app_id_;

    struct Info
    {
        std::string data;
        double create_ts, update_ts;

        Info():
            create_ts(0), update_ts(0)
        {}
        Info(const std::string &_data, double _create_ts, double _update_ts = 0):
            data(_data), create_ts(_create_ts),
            update_ts(_update_ts == 0? get_time(): _update_ts)
        {}
    };

    typedef std::map<std::string, Info> Storage;
    Storage storage_;

    double last_fetch_time_;
    double peer_timeout_;
    double refresh_interval_;
    std::string path_prefix_;
    std::vector<std::string> peer_uris_;

    Yb::Logger::Ptr log_;
    Yb::Mutex mutex_;

    typedef std::pair<std::string, Storage> PeerData;

    static const std::string format_ts(double ts);
    PeerData call_peer(const std::string &peer_uri,
            const std::string &method,
            const HttpParams &params,
            const std::string &http_method = "POST");
    PeerData read_peer(const std::string &peer_uri);
    void apply_update(const PeerData &peer_data);
    void push_to_peers();
    void fetch_data();
    void update_data_if_needed();

public:
    KeyKeeper(IConfig &cfg, Yb::ILogger &log);
    Yb::ElementTree::ElementPtr mk_resp(const std::string &status = "success");
    Yb::ElementTree::ElementPtr read();
    Yb::ElementTree::ElementPtr get();
    Yb::ElementTree::ElementPtr write(const Yb::StringDict &params, bool update = false);
    Yb::ElementTree::ElementPtr set(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr unset(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr cleanup(const Yb::StringDict &params);
};

extern KeyKeeper *key_keeper;

#endif // CARD_PROXY__KEY_KEEPER_LOGIC_H
// vim:ts=4:sts=4:sw=4:et:
