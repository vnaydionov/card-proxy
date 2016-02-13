// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "key_keeper_logic.h"
#include "utils.h"

#include <util/util_config.h>
#if defined(YBUTIL_WINDOWS)
#include <rpc.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

Yb::LongInt _get_random()
{
    Yb::LongInt buf;
#if defined(__WIN32__) || defined(_WIN32)
    UUID new_uuid;
    UuidCreate(&new_uuid);
    buf = new_uuid.Data1;
    buf <<= 32;
    Yb::LongInt buf2 = (new_uuid.Data2 << 16) | new_uuid.Data3;
    buf += buf2;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1)
        throw ::RunTimeError("can't open /dev/urandom");
    if (read(fd, &buf, sizeof(buf)) != sizeof(buf)) {
        close(fd);
        throw ::RunTimeError("can't read from /dev/urandom");
    }
    close(fd);
#endif
    return buf;
}

double get_time()
{
    return Yb::get_cur_time_millisec()/1000.;
}

const std::string KeyKeeper::format_ts(double ts)
{
    char buf[40];
    sprintf(buf, "%.3lf", ts);
    return std::string(buf);
}

KeyKeeper::PeerData KeyKeeper::call_peer(const std::string &peer_uri,
        const std::string &method,
        const HttpParams &params,
        const std::string &http_method)
{
    HttpResponse resp = http_post(
        peer_uri + path_prefix_ + method,
        NULL,
        peer_timeout_,
        http_method,
        HttpHeaders(),
        params);
    if (resp.get<0>() != 200)
        throw ::RunTimeError("call_peer: not HTTP 200");
    const std::string &body = resp.get<2>();
    auto root = Yb::ElementTree::parse(body);
    if (root->find_first("status")->get_text() != "success")
        throw ::RunTimeError("call_peer: not success");
    std::string app_id = root->find_first("app_id")->get_text();
    Storage storage;
    auto items_node = root->find_first("items");
    auto item_nodes = items_node->find_children("item");
    for (auto i = item_nodes->begin(), iend = item_nodes->end();
            i != iend; ++i)
    {
        auto &node = *i;
        const std::string &id = node->attrib_["id"];
        Info info(node->attrib_["data"],
                boost::lexical_cast<double>(node->attrib_["create_ts"]),
                boost::lexical_cast<double>(node->attrib_["update_ts"]));
        storage[id] = info;
    }
    return std::make_pair(app_id, storage);
}

KeyKeeper::PeerData KeyKeeper::read_peer(const std::string &peer_uri)
{
    return call_peer(peer_uri, "read", HttpParams(), "GET");
}

void KeyKeeper::apply_update(const KeyKeeper::PeerData &peer_data)
{
    Yb::ScopedLock lock(mutex_);
    for (auto i = peer_data.second.begin(), iend = peer_data.second.end();
            i != iend; ++i)
    {
        const auto &id = i->first;
        const auto &item = i->second;
        if (std::find(peer_uris_.begin(), peer_uris_.end(), id)
                == peer_uris_.end() ||
                storage_[id].create_ts < item.create_ts)
        {
            storage_[id] = item;
            storage_[id].update_ts = get_time();
        }
    }
}

void KeyKeeper::push_to_peers()
{
    std::vector<std::string> peer_uris;
    Storage storage;
    {
        Yb::ScopedLock lock(mutex_);
        peer_uris = peer_uris_;
        storage = storage_;
    }
    // craft params
    HttpParams params;
    int c = 0;
    for (auto i = storage.begin(), iend = storage.end();
            i != iend; ++i, ++c)
    {
        const auto &id = i->first;
        const auto &item = i->second;
        const auto suffix = "_" + Yb::to_string(c);
        params["id" + suffix] = id;
        params["data" + suffix] = item.data;
        params["create_ts" + suffix] = format_ts(item.create_ts);
    }
    for (auto i = peer_uris.begin(), iend = peer_uris.end();
            i != iend; ++i)
    {
        try {
            const auto &peer_uri = *i;
            call_peer(peer_uri, "write", params);
        }
        catch (const std::exception &e) {
            log_->error("push_to_peers: " + std::string(e.what()));
        }
    }
}

void KeyKeeper::fetch_data()
{
    std::vector<std::string> peer_uris, self_uris;
    {
        Yb::ScopedLock lock(mutex_);
        peer_uris = peer_uris_;
    }
    for (auto i = peer_uris.begin(), iend = peer_uris.end();
            i != iend; ++i)
    {
        const auto &peer_uri = *i;
        PeerData peer_data;
        try {
            peer_data = read_peer(peer_uri);
        }
        catch (const std::exception &e) {
            log_->error("fetch_data: " + std::string(e.what()));
        }
        if (peer_data.first == app_id_)
            self_uris.push_back(peer_uri);
        else
            apply_update(peer_data);
    }
    if (self_uris.size()) {
        std::string self_uris_str;
        for (auto i = self_uris.begin(), iend = self_uris.end();
                i != iend; ++i)
        {
            if (self_uris_str.size())
                self_uris_str += ", ";
            self_uris_str += '\"';
            self_uris_str += *i;
            self_uris_str += '\"';
        }
        self_uris_str = "[" + self_uris_str + "]";
        log_->info("Excluded URIs: " + self_uris_str);
        Yb::ScopedLock lock(mutex_);
        std::vector<std::string> new_peer_uris;
        for (auto i = peer_uris.begin(), iend = peer_uris.end();
                i != iend; ++i)
        {
            const auto &peer_uri = *i;
            if (std::find(self_uris.begin(), self_uris.end(), peer_uri)
                    == self_uris.end())
                new_peer_uris.push_back(peer_uri);
        }
        std::swap(peer_uris_, new_peer_uris);
    }
}

void KeyKeeper::update_data_if_needed()
{
    if (get_time() - last_fetch_time_ >= refresh_interval_) {
        last_fetch_time_ = get_time();
        fetch_data();
    }
}

KeyKeeper::KeyKeeper(IConfig &cfg, Yb::ILogger &log)
{
    app_key_ = Yb::to_string(_get_random());
    app_id_ = Yb::to_string(_get_random());
    last_fetch_time_ = 0;
    peer_timeout_ = cfg.get_value_as_int("Peers/Timeout")/1000.;
    refresh_interval_ = cfg.get_value_as_int("Peers/RefreshInterval")/1000.;
    path_prefix_ = cfg.get_value("HttpListener/Prefix");
    for (int i = 0; i < 10; ++i) {
        std::string key = "Peers/Peer" + Yb::to_string(i);
        if (cfg.has_key(key))
            peer_uris_.push_back(cfg.get_value(key));
    }
    log_.reset(log.new_logger("key_keeper").release());
}

Yb::ElementTree::ElementPtr KeyKeeper::mk_resp(const std::string &status)
{
    auto root = Yb::ElementTree::new_element("result");
    root->sub_element("status", status);
    root->sub_element("app_id", app_id_);
    return root;
}

Yb::ElementTree::ElementPtr KeyKeeper::read()
{
    auto resp = mk_resp();
    auto items_node = resp->sub_element("items");
    Yb::ScopedLock lock(mutex_);
    for (auto i = storage_.begin(), iend = storage_.end();
            i != iend; ++i)
    {
        auto item_node = items_node->sub_element("item");
        item_node->attrib_["id"] = i->first;
        item_node->attrib_["data"] = i->second.data;
        item_node->attrib_["create_ts"] = format_ts(i->second.create_ts);
        item_node->attrib_["update_ts"] = format_ts(i->second.update_ts);
    }
    return resp;
}

Yb::ElementTree::ElementPtr KeyKeeper::get()
{
    update_data_if_needed();
    return read();
}

Yb::ElementTree::ElementPtr KeyKeeper::write(const Yb::StringDict &params, bool update)
{
    Storage storage;
    if (update)
    {
        Yb::ScopedLock lock(mutex_);
        storage = storage_;
    }
    for (int i = -1; i < 100; ++i) {
        std::string suffix;
        if (i >= 0)
            suffix = "_" + Yb::to_string(i);
        if (params.find("id" + suffix) == params.end())
            continue;
        const auto &id = params.find("id" + suffix)->second;
        for (auto i = id.begin(), iend = id.end(); i != iend; ++i) {
            YB_ASSERT(('0' <= *i && *i <= '9') ||
                      ('a' <= *i && *i <= 'z') ||
                      ('A' <= *i && *i <= 'Z') || (*i == '_'));
        }
        YB_ASSERT(params.find("data" + suffix) != params.end());
        const auto &data = params.find("data" + suffix)->second;
        for (auto i = data.begin(), iend = data.end(); i != iend; ++i) {
            YB_ASSERT(0x20 <= *i && *i <= 0x7E);
        }
        YB_ASSERT(params.find("create_ts" + suffix) != params.end());
        double create_ts = boost::lexical_cast<double>(
                params.find("create_ts" + suffix)->second);
        storage[id] = Info(data, create_ts);
    }
    {
        Yb::ScopedLock lock(mutex_);
        std::swap(storage, storage_);
    }
    return mk_resp();
}


Yb::ElementTree::ElementPtr KeyKeeper::set(const Yb::StringDict &params)
{
    Yb::StringDict fixed_params = params;
    fixed_params["create_ts"] = format_ts(get_time());
    write(fixed_params, true);
    push_to_peers();
    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyKeeper::unset(const Yb::StringDict &params)
{
    YB_ASSERT(params.find("id") != params.end());
    const auto &id = params.find("id")->second;
    YB_ASSERT(storage_.find(id) != storage_.end());
    Yb::ScopedLock lock(mutex_);
    storage_.erase(storage_.find(id));
    push_to_peers();
    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyKeeper::cleanup(const Yb::StringDict &params)
{
    YB_ASSERT(params.find("id") != params.end());
    const auto &id = params.find("id")->second;
    YB_ASSERT(storage_.find(id) != storage_.end());
    Yb::ScopedLock lock(mutex_);
    Storage new_storage;
    new_storage[id] = storage_[id];
    std::swap(new_storage, storage_);
    push_to_peers();
    return mk_resp();
}

KeyKeeper *key_keeper = NULL;

// vim:ts=4:sts=4:sw=4:et:
