#ifndef _CARD_PROXY__APP_H_
#define _CARD_PROXY__APP_H_

#include <memory>
#include <string>
#include <fstream>
#include <util/nlogger.h>
#include <util/element_tree.h>
#include <util/singleton.h>
#include <orm/data_object.h>

#define SETTINGS_FILE "settings.xml"

class AppSettings {
    Yb::ElementTree::ElementPtr root_;
    std::string file_name_;
    bool modified_;

public:
    AppSettings(const std::string &file_name = SETTINGS_FILE);
    ~AppSettings();

    void fill_tree();
    void save_to_xml();
    const std::string to_string() const;

    const int get_card_proxy_port();
    const std::string get_card_proxy_prefix();

    const std::string get_key_keeper_server();
    const std::string get_key_keeper_port();
    const std::string get_key();
    
    const int get_dek_use_count();
    const int get_dek_bot_limit();
    const int get_dek_top_limit();
};


class App: public Yb::ILogger
{
    std::auto_ptr<std::ofstream> file_stream_;
    std::auto_ptr<Yb::LogAppender> appender_;
    Yb::ILogger::Ptr log_;
    std::auto_ptr<Yb::Engine> engine_;

    void init_log(const std::string &log_name);
    void init_engine(const std::string &db_name);
public:
    App() {}
    void init(const std::string &log_name = "log.txt",
            const std::string &db_name = "db");
    virtual ~App();
    Yb::Engine *get_engine();
    std::auto_ptr<Yb::Session> new_session();
    Yb::ILogger::Ptr new_logger(const std::string &name);
    void log(int level, const std::string &msg);
    const std::string get_name() const;
};

typedef Yb::SingletonHolder<App> theApp;

#endif // _CARD_PROXY__APP_H_
// vim:ts=4:sts=4:sw=4:et:
