// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "conf_reader.h"
#include <fstream>
#include <stdexcept>
#include <util/string_utils.h>

IConfig::~IConfig() {}

void IConfig::reload() {}

int IConfig::get_value_as_int(const Yb::String &key)
{
    Yb::String value = get_value(key);
    int result = -1;
    Yb::from_string(value, result);
    return result;
}

bool IConfig::get_value_as_bool(const Yb::String &key)
{
    Yb::String value = Yb::StrUtils::str_to_upper(get_value(key));
    return value == _T("1") || value == _T("YES") || value == _T("ON") ||
           value == _T("TRUE") || value == _T("Y") || value == _T("T");
}

Yb::ElementTree::ElementPtr XmlConfig::load_tree(const Yb::String &fname)
{
    std::ifstream file(NARROW(fname).c_str());
    if (!file.good())
        throw std::runtime_error("Cannot read XML config file: " + fname);
    Yb::ElementTree::ElementPtr root = Yb::ElementTree::parse(file);
    return root;
}

XmlConfig::XmlConfig(const Yb::String &fname)
    : fname_(fname)
    , config_(load_tree(fname))
{}

void XmlConfig::reload()
{
    config_ = load_tree(fname_);
}

const Yb::String XmlConfig::get_value(const Yb::String &key)
{
    Yb::Strings parts;
    Yb::StrUtils::split_str(key, _T("/"), parts);
    Yb::ElementTree::ElementPtr cur_node = config_;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        cur_node = cur_node->find_first(parts[i]);
    }
    return cur_node->get_text();
}

bool XmlConfig::has_key(const Yb::String &key)
{
    try {
        get_value(key);
        return true;
    }
    catch (const Yb::ElementTree::ElementNotFound &) { }
    return false;
}

EnvConfig::EnvConfig(const Yb::String &prefix)
    : prefix_(prefix)
{}

const Yb::String EnvConfig::get_value(const Yb::String &key)
{
    Yb::String env_key = prefix_ + key;
    char *x = getenv(NARROW(env_key).c_str());
    if (!x)
        throw std::runtime_error("No environment variable: " + NARROW(env_key));
    return Yb::StrUtils::xgetenv(env_key);
}

bool EnvConfig::has_key(const Yb::String &key)
{
    try {
        get_value(key);
        return true;
    }
    catch (const std::exception &) { }
    return false;
}

// vim:ts=4:sts=4:sw=4:et:
