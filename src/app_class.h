// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__APP_CLASS_H
#define CARD_PROXY__APP_CLASS_H

#include <memory>
#include <string>
#include <fstream>
#include <util/nlogger.h>
#include <util/singleton.h>
#include <orm/data_object.h>
#include "conf_reader.h"

class App: public Yb::ILogger
{
    IConfig::Ptr config_;
    std::auto_ptr<std::ofstream> file_stream_;
    std::auto_ptr<Yb::LogAppender> appender_;
    Yb::ILogger::Ptr log_;
    std::auto_ptr<Yb::Engine> engine_;
    std::auto_ptr<Yb::Session> session_;

    void init_log(const std::string &log_name);
    void init_engine(const std::string &db_name);
    void init_dek_pool();
public:
    App() {}
    void init(IConfig::Ptr config);
    virtual ~App();
    IConfig *cfg();
    Yb::Engine *get_engine();
    std::auto_ptr<Yb::Session> new_session();
    Yb::ILogger::Ptr new_logger(const std::string &name);
    void log(int level, const std::string &msg);
    const std::string get_name() const;
};

typedef Yb::SingletonHolder<App> theApp;

#endif // CARD_PROXY__APP_CLASS_H
// vim:ts=4:sts=4:sw=4:et:
