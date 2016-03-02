// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "keyapi_logic.h"
#include "utils.h"
#include "app_class.h"
#include "tokenizer.h"
#include <boost/regex.hpp>
#include <stdio.h>

#include "domain/Config.h"

static const boost::regex id_fmt("\\w{1,40}");
static const boost::regex data_fmt("[\x20-\x7F]{0,2000}");
static const boost::regex fmt_re("id_(\\d{1,9})");


const std::map<std::string, std::string> SERVANTS = {
    {"card_proxy_tokenizer",
     "/etc/card_proxy_common/key_settings.cfg.xml"},

/*
    {"card_proxy_keyapi",
     "/etc/card_proxy_common/key_settings.cfg.xml"},
*/
};

const std::string KEY_API_HELPER =
    "/usr/bin/card_proxy_keyapi-helper";


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

void KeyAPI::call_helper(
        const std::string &servant,
        const std::string &config_path,
        const std::string &xml_s)
{
    char tmp_name[] = "/tmp/keyapi-XXXXXX";
    int tmp_fd = ::mkstemp(tmp_name);
    if (tmp_fd == -1) {
        throw ::RunTimeError("KeyAPI::call_helper(): "
                "can't create a temp file");
    }
    FILE *tmp_f = NULL;
    try {
        tmp_f = ::fdopen(tmp_fd, "wb");
        if (!tmp_f)
            throw ::RunTimeError("KeyAPI::call_helper(): "
                    "can't fdopen()");
        tmp_fd = -1;
        int res = ::fwrite(xml_s.c_str(), 1, xml_s.size(), tmp_f);
        if (res != (int)xml_s.size())
            throw ::RunTimeError("KeyAPI::call_helper(): "
                    "fwrite() error");
        if (::fclose(tmp_f) != 0)
            throw ::RunTimeError("KeyAPI::call_helper(): "
                    "failed to close file stream");
        tmp_f = NULL;
        const std::string cmd_line =
                        std::string("/usr/bin/sudo ") +
                        KEY_API_HELPER + " " +
                        servant + " " +
                        config_path + " " +
                        tmp_name;
        log_->debug("cmd_line: " + cmd_line);
        res = ::system(cmd_line.c_str());
        log_->debug("exit code: " + Yb::to_string(res));
        if (res != 0)
            throw ::RunTimeError("KeyAPI::call_helper(): "
                    "failed to execute the helper");
        res = ::remove(tmp_name);
        tmp_name[0] = 0;
        if (res != 0)
            throw ::RunTimeError("KeyAPI::call_helper(): "
                    "failed to remove temp file");
    }
    catch (const std::exception &e) {
        log_->error("calling helper failed: " + std::string(e.what()));
        if (tmp_f) {
            if (::fclose(tmp_f) != 0)
                log_->error("KeyAPI::call_helper(): "
                        "failed to close file stream");
        }
        else if (tmp_fd != -1) {
            if (::close(tmp_fd) != 0)
                log_->error("KeyAPI::call_helper(): "
                        "failed to close file descriptor");
        }

        if (tmp_name[0]) {
            if (::remove(tmp_name) != 0)
                log_->error("KeyAPI::call_helper(): "
                        "failed to remove temp file");
        }
        throw;
    }
}

static Yb::ElementTree::ElementPtr find_item(
        Yb::ElementTree::ElementPtr items, const std::string &id)
{
    auto found = items->find_all("item");
    for (auto i = found->begin(), iend = found->end(); i != iend; ++i)
    {
        if ((*i)->attrib_.get("id", "") == id)
            return *i;
    }
    Yb::ElementTree::ElementPtr null;
    return null;
}

const std::string KeyAPI::process_path(
        const std::string &servant,
        const std::string &config_path,
        const std::string &mode,
        const std::string &id,
        const std::string &data)
{
    log_->info("Processing path " + config_path);
    Yb::ElementTree::ElementPtr root =
        Yb::ElementTree::parse_file(config_path);
    auto items = root->find_first("items");
    auto ts = KeyAPI::format_ts(KeyAPI::get_time());

    if (mode == "set") {
        auto item = find_item(items, id);
        if (!item.get()) {
            item = items->sub_element("item");
            item->attrib_["id"] = id;
        }
        item->attrib_["data"] = data;
        item->attrib_["update_ts"] = ts;
    }

    else if (mode == "unset") {
        auto item = find_item(items, id);
        if (item.get()) {
            items->children_.erase(
                    std::remove(items->children_.begin(),
                                items->children_.end(),
                                item),
                    items->children_.end());
        }
    }

    else if (mode == "cleanup") {
        auto item = find_item(items, id);
        std::string new_data, new_update_ts;
        if (item.get()) {
            new_data = item->attrib_.get("data", "");
            new_update_ts = item->attrib_.get("update_ts", "");
        }
        root->children_.clear();
        items = root->sub_element("items");
        if (!new_data.empty()) {
            auto new_item = items->sub_element("item");
            new_item->attrib_["id"] = id;
            new_item->attrib_["data"] = new_data;
            new_item->attrib_["update_ts"] = new_update_ts;
        }
    }

    if (mode != "get") {
        auto s = root->serialize();
        call_helper(servant, config_path, s);
    }

    return items->serialize();
}

Yb::ElementTree::ElementPtr KeyAPI::process_cmd(
    const std::string &cmd,
    const std::string &id,
    const std::string &data)
{
    bool success = true;
    auto resp = mk_resp();
    auto servants_node = resp->sub_element("servants");
    auto i = SERVANTS.begin(), iend = SERVANTS.end();
    for (; i != iend; ++i) {
        const auto &servant = i->first;
        const auto &config_path = i->second;
        auto servant_node = servants_node->sub_element("servant");
        servant_node->attrib_["name"] = servant;
        servant_node->attrib_["config_path"] = config_path;
        try {
            auto items_s = process_path(
                    servant, config_path, cmd, id, data);
            auto items_node = Yb::ElementTree::parse(items_s);
            servant_node->children_.push_back(items_node);
        }
        catch (const std::exception &e) {
            success = false;
            servant_node->sub_element("exception", std::string(e.what()));
        }
    }
    if (!success)
        resp->find_first("status")->set_text("fail");
    return resp;
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

const std::string KeyAPI::get_state()
{
    return get_config_param("STATE");
}

void KeyAPI::set_state(const std::string &state)
{
    set_config_param("STATE", state);
}

std::pair<int, int> KeyAPI::kek_auth(const std::string &mode,
        const std::string &password, int kek_version)
{
    YB_ASSERT(password.size() == 64);
    YB_ASSERT(mode == "CURRENT" || mode == "LAST" ||
              mode == "TARGET" || mode == "CUSTOM");
    TokenizerConfig tcfg;
    if (mode == "CURRENT")
        kek_version = tcfg.get_current_version();
    else if (mode == "LAST")
        kek_version = tcfg.get_last_version();
    else
        kek_version = tcfg.get_switch_version();
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
    return root;
}

Yb::ElementTree::ElementPtr KeyAPI::generate_kek(const Yb::StringDict &params)
{
    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::get_component(const Yb::StringDict &params)
{
    auto password = params.get("password", "");
    auto want_kek_version_str = params.get("version", "");
    auto r = kek_auth("CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    TokenizerConfig tcfg;
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
    auto password = params.get("password", "");
    auto want_kek_version_str = params.get("version", "");
    std::string mode = "LAST";
    int want_kek_version = -1;
    if (!want_kek_version_str.empty()) {
        mode = "CUSTOM";
        want_kek_version = boost::lexical_cast<int>(want_kek_version_str);
    }
    auto r = kek_auth(mode, password, want_kek_version);
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
    auto password = params.get("password", "");
    int want_kek_version = boost::lexical_cast<int>(params.get("version", ""));
    auto r = kek_auth("CURRENT", password);
    int kek_version = r.first;
    YB_ASSERT(kek_version >= 0);
    int part_n = r.second;
    YB_ASSERT(part_n >= 1 && part_n <= 3);
    TokenizerConfig tcfg;
    YB_ASSERT(tcfg.is_kek_valid(want_kek_version));
    YB_ASSERT(tcfg.is_version_checked(want_kek_version));
    set_config_param("KEK_TARGET_VERSION", Yb::to_string(want_kek_version));
    auto resp = mk_resp();
    resp->sub_element("version", Yb::to_string(kek_version));
    resp->sub_element("part", Yb::to_string(part_n));
    resp->sub_element("target_version", Yb::to_string(want_kek_version));
    return resp;
}

Yb::ElementTree::ElementPtr KeyAPI::cleanup(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;
    YB_ASSERT(regex_match(id, id_fmt, boost::format_perl));

    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::reencrypt_deks(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;
    YB_ASSERT(regex_match(id, id_fmt, boost::format_perl));

    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::switch_kek(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;
    YB_ASSERT(regex_match(id, id_fmt, boost::format_perl));

    return mk_resp();
}

Yb::ElementTree::ElementPtr KeyAPI::status(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;
    YB_ASSERT(regex_match(id, id_fmt, boost::format_perl));

    return mk_resp();
}

// vim:ts=4:sts=4:sw=4:et:
