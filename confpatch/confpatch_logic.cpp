// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "confpatch_logic.h"
#include "utils.h"
#include "app_class.h"
#include <boost/regex.hpp>
#include <stdio.h>

static const boost::regex id_fmt("\\w{1,40}");
static const boost::regex data_fmt("[\x20-\x7F]{0,2000}");
static const boost::regex fmt_re("id_(\\d{1,9})");


const std::map<std::string, std::string> SERVANTS = {
    {"card_proxy_tokenizer",
     "/etc/card_proxy_common/key_settings.cfg.xml"},

    {"card_proxy_keyapi",
     "/etc/card_proxy_common/key_settings.cfg.xml"},
};

const std::string CONF_PATCH_HELPER =
    "/usr/bin/card_proxy_confpatch-helper";


double ConfPatch::get_time()
{
    return Yb::get_cur_time_millisec()/1000.;
}

const std::string ConfPatch::format_ts(double ts)
{
    char buf[40];
    sprintf(buf, "%.3lf", ts);
    return std::string(buf);
}

void ConfPatch::call_helper(
        const std::string &servant,
        const std::string &config_path,
        const std::string &xml_s)
{
    char tmp_name[] = "/tmp/confpatch-XXXXXX";
    int tmp_fd = ::mkstemp(tmp_name);
    if (tmp_fd == -1) {
        throw ::RunTimeError("ConfPatch::call_helper(): "
                "can't create a temp file");
    }
    FILE *tmp_f = NULL;
    try {
        tmp_f = ::fdopen(tmp_fd, "wb");
        if (!tmp_f)
            throw ::RunTimeError("ConfPatch::call_helper(): "
                    "can't fdopen()");
        tmp_fd = -1;
        int res = ::fwrite(xml_s.c_str(), 1, xml_s.size(), tmp_f);
        if (res != (int)xml_s.size())
            throw ::RunTimeError("ConfPatch::call_helper(): "
                    "fwrite() error");
        if (::fclose(tmp_f) != 0)
            throw ::RunTimeError("ConfPatch::call_helper(): "
                    "failed to close file stream");
        tmp_f = NULL;
        const std::string cmd_line =
                        std::string("/usr/bin/sudo ") +
                        CONF_PATCH_HELPER + " " +
                        servant + " " +
                        config_path + " " +
                        tmp_name;
        log_->debug("cmd_line: " + cmd_line);
        res = ::system(cmd_line.c_str());
        log_->debug("exit code: " + Yb::to_string(res));
        if (res != 0)
            throw ::RunTimeError("ConfPatch::call_helper(): "
                    "failed to execute the helper");
        res = ::remove(tmp_name);
        tmp_name[0] = 0;
        if (res != 0)
            throw ::RunTimeError("ConfPatch::call_helper(): "
                    "failed to remove temp file");
    }
    catch (const std::exception &e) {
        log_->error("calling helper failed: " + std::string(e.what()));
        if (tmp_f) {
            if (::fclose(tmp_f) != 0)
                log_->error("ConfPatch::call_helper(): "
                        "failed to close file stream");
        }
        else if (tmp_fd != -1) {
            if (::close(tmp_fd) != 0)
                log_->error("ConfPatch::call_helper(): "
                        "failed to close file descriptor");
        }

        if (tmp_name[0]) {
            if (::remove(tmp_name) != 0)
                log_->error("ConfPatch::call_helper(): "
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

const std::string ConfPatch::process_path(
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
    auto ts = ConfPatch::format_ts(ConfPatch::get_time());

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

Yb::ElementTree::ElementPtr ConfPatch::process_cmd(
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

ConfPatch::ConfPatch(IConfig &cfg, Yb::ILogger &log)
{
    log_.reset(log.new_logger("confpatch").release());
}

Yb::ElementTree::ElementPtr ConfPatch::mk_resp(const std::string &status)
{
    auto root = Yb::ElementTree::new_element("result");
    root->sub_element("status", status);
    root->sub_element("ts", format_ts(get_time()));
    return root;
}

Yb::ElementTree::ElementPtr ConfPatch::get()
{
    return process_cmd("get", "", "");
}

Yb::ElementTree::ElementPtr ConfPatch::set(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;
    YB_ASSERT(regex_match(id, id_fmt, boost::format_perl));

    j = params.find("data");
    YB_ASSERT(j != params.end());
    const auto &data = j->second;
    YB_ASSERT(regex_match(data, data_fmt, boost::format_perl));

    return process_cmd("set", id, data);
}

Yb::ElementTree::ElementPtr ConfPatch::unset(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;
    YB_ASSERT(regex_match(id, id_fmt, boost::format_perl));

    return process_cmd("unset", id, "");
}

Yb::ElementTree::ElementPtr ConfPatch::cleanup(const Yb::StringDict &params)
{
    auto j = params.find("id");
    YB_ASSERT(j != params.end());
    const auto &id = j->second;
    YB_ASSERT(regex_match(id, id_fmt, boost::format_perl));

    return process_cmd("unset", id, "");
}

// vim:ts=4:sts=4:sw=4:et:
