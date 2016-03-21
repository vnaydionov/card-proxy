// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef _AUTH__APP_H_
#define _AUTH__APP_H_

#include <memory>
#include <string>
#include <fstream>
#include <util/nlogger.h>
#include <util/singleton.h>
#include <orm/data_object.h>
#include "conf_reader.h"

const std::string filter_log_msg(const std::string &msg);

class FileLogAppender: public Yb::LogAppender
{
public:
    FileLogAppender(std::ostream &out);
    void append(const Yb::LogRecord &rec);
};

class SyslogAppender: public Yb::ILogAppender
{
    static char process_name[100];

    Yb::Mutex appender_mutex_;
    typedef std::map<std::string, int> LogLevelMap;
    LogLevelMap log_levels_;

    static int log_level_to_syslog(int log_level);
    void really_append(const Yb::LogRecord &rec);

public:
    SyslogAppender();
    ~SyslogAppender();
    void append(const Yb::LogRecord &rec);
    int get_level(const std::string &name);
    void set_level(const std::string &name, int level);
};

class App: public Yb::ILogger
{
    IConfig::Ptr config_;
    std::auto_ptr<std::ofstream> file_stream_;
    std::auto_ptr<Yb::ILogAppender> appender_;
    Yb::ILogger::Ptr log_;
    bool use_db_;
    std::auto_ptr<Yb::Engine> engine_;

    void init_log(const std::string &log_name,
                  const std::string &log_level);
    void init_engine(const std::string &db_name);
    const std::string get_db_url();
public:
    App(): use_db_(true) {}
    void init(IConfig::Ptr config, bool use_db = true);
    virtual ~App();
    IConfig &cfg();
    Yb::Engine &get_engine();
    bool uses_db() const { return use_db_; }
    std::auto_ptr<Yb::Session> new_session();

    // implement ILogger
    Yb::ILogger::Ptr new_logger(const std::string &name);
    Yb::ILogger::Ptr get_logger(const std::string &name);
    int get_level();
    void set_level(int level);
    void log(int level, const std::string &msg);
    const std::string get_name() const;
};

typedef Yb::SingletonHolder<App> theApp;

#endif // _AUTH__APP_H_
// vim:ts=4:sts=4:sw=4:et:
