// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__CONF_READER_H
#define CARD_PROXY__CONF_READER_H

#include <memory>
#include <string>
#include <util/element_tree.h>
#include <util/thread.h>

class IConfig
{
public:
    typedef std::auto_ptr<IConfig> Ptr;

    virtual ~IConfig();
    virtual void reload();
    virtual const Yb::String get_value(const Yb::String &key) = 0;
    virtual bool has_key(const Yb::String &key) = 0;

    int get_value_as_int(const Yb::String &key);
    bool get_value_as_bool(const Yb::String &key);
};

class XmlConfig: public IConfig
{
    const Yb::String fname_;
    Yb::ElementTree::ElementPtr config_;
    Yb::Mutex config_mux_;

    static Yb::ElementTree::ElementPtr load_tree(const Yb::String &fname);
    // non-copyable
    XmlConfig(const XmlConfig &);
    XmlConfig &operator=(const XmlConfig &);
public:
    XmlConfig(const Yb::String &fname);
    virtual void reload();
    virtual const Yb::String get_value(const Yb::String &key);
    virtual bool has_key(const Yb::String &key);
};

class EnvConfig: public IConfig
{
    const Yb::String prefix_;
public:
    EnvConfig(const Yb::String &prefix);
    virtual const Yb::String get_value(const Yb::String &key);
    virtual bool has_key(const Yb::String &key);
};

#endif // CARD_PROXY__CONF_READER_H
// vim:ts=4:sts=4:sw=4:et:
