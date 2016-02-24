// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__CONF_PATCH_LOGIC_H
#define CARD_PROXY__CONF_PATCH_LOGIC_H

#include <string>
#include <map>
#include <stdexcept>
#include <util/nlogger.h>
#include <util/element_tree.h>
#include "conf_reader.h"
#include "http_post.h"

class ConfPatch
{
    Yb::Logger::Ptr log_;

    static double get_time();
    static const std::string format_ts(double ts);

    void call_helper(
        const std::string &servant,
        const std::string &config_path,
        const std::string &xml_s);

    const std::string process_path(
        const std::string &servant,
        const std::string &config_path,
        const std::string &mode,
        const std::string &id,
        const std::string &data);

    Yb::ElementTree::ElementPtr process_cmd(
        const std::string &cmd,
        const std::string &id,
        const std::string &data);

    ConfPatch(const ConfPatch &);
    ConfPatch &operator=(const ConfPatch &);

public:
    ConfPatch(IConfig &cfg, Yb::ILogger &log);
    Yb::ElementTree::ElementPtr mk_resp(const std::string &status = "success");
    Yb::ElementTree::ElementPtr get();
    Yb::ElementTree::ElementPtr set(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr unset(const Yb::StringDict &params);
    Yb::ElementTree::ElementPtr cleanup(const Yb::StringDict &params);
};

#endif // CARD_PROXY__CONF_PATCH_LOGIC_H
// vim:ts=4:sts=4:sw=4:et:
