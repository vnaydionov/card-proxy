#include "app_class.h"
#include <fstream>
#include <stdexcept>
#include <boost/regex.hpp>
#include <util/string_utils.h>
#include <orm/domain_object.h>
#include "utils.h"
#if !defined(YBUTIL_WINDOWS)
#include <syslog.h>
#endif

const std::string escape_nl(const std::string &msg)
{
    std::string result;
    result.reserve(msg.size() + 4*10);
    char buf[40];
    auto i = msg.begin(), iend = msg.end();
    for (; i != iend; ++i) {
        if (!(*i >= '\040' && *i <= '\176')) {
            // not a printable character
            snprintf(buf, sizeof(buf), "#%03o", (*i & 0xff));
            buf[sizeof(buf) - 1] = 0;
            result += buf;
        }
        else
            result += *i;
    }
    return result;
}

const std::string filter_log_msg(const std::string &msg)
{
    std::string fixed_msg = msg;

    // parts of master key
    static const boost::regex *key_re = NULL;
    if (!key_re)
        key_re = new boost::regex(
                "(?:(?<=[^0-9a-fA-F])|^)([0-9a-fA-F]{8})([0-9a-fA-F]{48})([0-9a-fA-F]{8})(?=[^0-9a-fA-F]|$)");
    std::string key_re_sub = "\\1....\\3";
    fixed_msg = boost::regex_replace(fixed_msg, *key_re, key_re_sub,
            boost::match_default | boost::format_perl);

    // search tokens
    static const boost::regex *token_re = NULL;
    if (!token_re)
        token_re = new boost::regex(
                "(?:(?<=[^0-9a-fA-F])|^)([a-fA-F0-9]{4})([a-fA-F0-9]{24})([a-fA-F0-9]{4})(?=[^0-9a-fA-F]|$)");
    std::string token_re_sub = "\\1xxxx\\3";
    fixed_msg = boost::regex_replace(fixed_msg, *token_re, token_re_sub,
            boost::match_default | boost::format_perl);

    // hmacs
    static const boost::regex *hmac_re = NULL;
    if (!hmac_re)
        hmac_re = new boost::regex(
                "(?:(?<=[^a-zA-Z0-9+/])|^)([a-zA-Z0-9+/]{4})([a-zA-Z0-9+/]{36})([a-zA-Z0-9+/]{3}=)");
    std::string hmac_re_sub = "\\1????\\3";
    fixed_msg = boost::regex_replace(fixed_msg, *hmac_re, hmac_re_sub,
            boost::match_default | boost::format_perl);

    const std::string pan_char_re_str = "(?:[0-9][^:a-tv-zA-TV-Z0-9]{0,5})";

    // cards 16-19
    static const boost::regex *card_re = NULL;
    if (!card_re)
        card_re = new boost::regex(
                replace_str(
                    "(?:(?<=[^\\d])|^)(PCHAR{6})(PCHAR{6,9})(PCHAR{4})(?=[^\\d]|$)",
                    "PCHAR", pan_char_re_str
                ));
    const std::string card_re_sub = "\\1XXXX\\3";
    fixed_msg = boost::regex_replace(fixed_msg, *card_re, card_re_sub,
            boost::match_default | boost::format_perl);

    // cards 12-15
    static const boost::regex *short_card_re = NULL;
    if (!short_card_re)
        short_card_re = new boost::regex(
                replace_str(
                    "(?:(?<=[^\\d])|^)(PCHAR{2})(PCHAR{6,9})(PCHAR{4})(?=[^\\d]|$)",
                    "PCHAR", pan_char_re_str
                ));
    const std::string short_card_re_sub = "\\1XXXX\\3";
    fixed_msg = boost::regex_replace(fixed_msg, *short_card_re, short_card_re_sub,
            boost::match_default | boost::format_perl);

    // cvn json
    static const boost::regex *cvn_json_re = NULL;
    if (!cvn_json_re)
        cvn_json_re = new boost::regex(
                "([\'\"][cC][vV][cCvVnN]2?[\'\"]):( *)([\'\"]\\d{3}[\'\"])");
    std::string cvn_json_re_sub("\\1:\\2\"XXX\"");
    fixed_msg = boost::regex_replace(fixed_msg, *cvn_json_re, cvn_json_re_sub,
            boost::match_default | boost::format_perl);

    // cvn form-encoded
    static const boost::regex *cvn_url_re = NULL;
    if (!cvn_url_re)
        cvn_url_re = new boost::regex(
                "([cC][vV][cCvVnN]2?)=(\\d{3})(?=[^\\d]|$)");
    std::string cvn_url_re_sub("\\1=XXX");
    fixed_msg = boost::regex_replace(fixed_msg, *cvn_url_re, cvn_url_re_sub,
            boost::match_default | boost::format_perl);

    return fixed_msg;
}

using namespace std;

FileLogAppender::FileLogAppender(std::ostream &out)
    : LogAppender(out)
{}

void FileLogAppender::append(const Yb::LogRecord &rec)
{
    Yb::LogRecord new_rec(rec.get_level(),
                          rec.get_component(),
                          filter_log_msg(rec.get_msg()));
    Yb::LogAppender::append(new_rec);
}

#if !defined(YBUTIL_WINDOWS)
char SyslogAppender::process_name[100];

int SyslogAppender::log_level_to_syslog(int log_level)
{
    switch (log_level) {
    case Yb::ll_CRITICAL:
        return LOG_CRIT;
    case Yb::ll_ERROR:
        return LOG_ERR;
    case Yb::ll_WARNING:
        return LOG_WARNING;
    case Yb::ll_INFO:
        return LOG_INFO;
    case Yb::ll_DEBUG:
    case Yb::ll_TRACE:
        return LOG_DEBUG;
    }
    return LOG_DEBUG;
}

SyslogAppender::SyslogAppender()
{
    std::string process = get_process_name();
    strncpy(process_name, process.c_str(), sizeof(process_name));
    process_name[sizeof(process_name) - 1] = 0;
    ::openlog(process_name, LOG_NDELAY | LOG_PID, LOG_USER);
}

SyslogAppender::~SyslogAppender()
{
    ::closelog();
}

void SyslogAppender::really_append(const Yb::LogRecord &rec)
{
    int priority = log_level_to_syslog(rec.get_level());
    std::ostringstream msg;
    msg << "T" << rec.get_tid() << " "
        << rec.get_component() << ": "
        << escape_nl(filter_log_msg(rec.get_msg()));
    ::syslog(priority, fmt_string_escape(msg.str()).c_str());
}

void SyslogAppender::append(const Yb::LogRecord &rec)
{
    Yb::ScopedLock lk(appender_mutex_);
    int target_level = Yb::ll_ALL;
    LogLevelMap::iterator it = log_levels_.find(rec.get_component());
    if (log_levels_.end() != it)
        target_level = it->second;
    if (rec.get_level() <= target_level) {
        really_append(rec);
    }
}

int SyslogAppender::get_level(const std::string &name)
{
    Yb::ScopedLock lk(appender_mutex_);
    LogLevelMap::iterator it = log_levels_.find(name);
    if (log_levels_.end() == it)
        return Yb::ll_ALL;
    return it->second;
}

void SyslogAppender::set_level(const std::string &name, int level)
{
    Yb::ScopedLock lk(appender_mutex_);
    if (name.size() >= 2 && name.substr(name.size() - 2) == ".*")
    {
        std::string prefix = name.substr(0, name.size() - 1);
        LogLevelMap::iterator it = log_levels_.begin(),
                              it_end = log_levels_.end();
        for (; it != it_end; ++it)
            if (it->first.substr(0, prefix.size()) == prefix)
                it->second = level;
        std::string prefix0 = name.substr(0, name.size() - 2);
        log_levels_[prefix0] = level;
    }
    else {
        LogLevelMap::iterator it = log_levels_.find(name);
        if (log_levels_.end() == it)
            log_levels_[name] = level;
        else
            it->second = level;
    }
}
#endif // !defined(YBUTIL_WINDOWS)

int decode_log_level(const string &log_level0)
{
    using Yb::StrUtils::str_to_lower;
    const string log_level = NARROW(str_to_lower(WIDEN(log_level0)));
    if (log_level.empty())
        return Yb::ll_DEBUG;
    if (log_level == "critical" || log_level == "cri" || log_level == "crit")
        return Yb::ll_CRITICAL;
    if (log_level == "error"    || log_level == "err" || log_level == "erro")
        return Yb::ll_ERROR;
    if (log_level == "warning"  || log_level == "wrn" || log_level == "warn")
        return Yb::ll_WARNING;
    if (log_level == "info"     || log_level == "inf")
        return Yb::ll_INFO;
    if (log_level == "debug"    || log_level == "dbg" || log_level == "debg")
        return Yb::ll_DEBUG;
    if (log_level == "trace"    || log_level == "trc" || log_level == "trac")
        return Yb::ll_TRACE;
    throw RunTimeError("invalid log level: " + log_level);
}

const string encode_log_level(int level)
{
    Yb::LogRecord r(level, "__xxx__", "yyy");
    return string(r.get_level_name());
}

void App::init_log(const string &log_name, const string &log_level)
{
    using Yb::StrUtils::split_str_by_chars;
    using Yb::StrUtils::trim_trailing_space;
    if (!log_.get()) {
#if !defined(YBUTIL_WINDOWS)
        using Yb::StrUtils::str_to_lower;
        if (_T("syslog") == str_to_lower(WIDEN(log_name))) {
            appender_.reset(new SyslogAppender());
        }
        else {
#endif // !defined(YBUTIL_WINDOWS)
            file_stream_.reset(new ofstream(log_name.data(), ios::app));
            if (file_stream_->fail())
                throw RunTimeError("can't open logfile: " + log_name);
            appender_.reset(new FileLogAppender(*file_stream_));
#if !defined(YBUTIL_WINDOWS)
        }
#endif // !defined(YBUTIL_WINDOWS)
        log_.reset(new Yb::Logger(appender_.get()));
        info("Application started.");
        info("Setting level " + encode_log_level(decode_log_level(log_level))
                + " for root logger");
        log_->set_level(decode_log_level(log_level));
        const string log_level = cfg().get_value("LogLevel");
        vector<string> target_levels;
        split_str_by_chars(log_level, ",", target_levels);
        auto i = target_levels.begin(), iend = target_levels.end();
        for (; i != iend; ++i) {
            auto &target_level = *i;
            vector<string> parts;
            split_str_by_chars(target_level, ":", parts, 2);
            if (parts.size() == 2) {
                auto target = trim_trailing_space(parts[0]);
                auto level = trim_trailing_space(parts[1]);
                info("Setting level " + encode_log_level(decode_log_level(level))
                        + " for log target " + target);
                log_->get_logger(target)->set_level(decode_log_level(level));
            }
        }
    }
}

const Yb::String App::get_db_url()
{
    const string type = cfg().get_value("DbBackend/@type");
    const string db = cfg().get_value("DbBackend/DB");
    const string user = cfg().get_value("DbBackend/User");
    const string pass = cfg().get_value("DbBackend/Pass");
    if (type == "mysql+soci") {
        const string host = cfg().get_value("DbBackend/Host");
        const int port = cfg().get_value_as_int("DbBackend/Port");
        return type + "://user=" + user +
            " pass=" + pass +
            " host=" + host +
            " port=" + Yb::to_string(port) +
            " service=" + db +
            " charset=utf8";
    }
    if (type == "mysql+odbc") {
        return type + "://" + user + ":" + pass + "@" + db;
    }
    throw RunTimeError("invalid DbBackend configuration");
}

void App::init_engine(const Yb::String &db_name)
{
    if (!engine_.get()) {
        Yb::ILogger::Ptr yb_logger(new_logger("yb").release());
        Yb::init_schema();
        auto_ptr<Yb::SqlPool> pool(
                new Yb::SqlPool(
                    YB_POOL_MAX_SIZE, YB_POOL_IDLE_TIME,
                    YB_POOL_MONITOR_SLEEP, yb_logger.get(), false));
        Yb::SqlSource src(get_db_url());
        src[_T("&id")] = db_name;
        pool->add_source(src);
        engine_.reset(new Yb::Engine(Yb::Engine::READ_WRITE, pool, db_name));
        engine_->set_echo(true);
        engine_->set_logger(yb_logger);
    }
}

void App::init(IConfig::Ptr config, bool use_db)
{
    config_.reset(config.release());
    init_log(cfg().get_value("Log"), cfg().get_value("Log/@level"));

    try {
        env_type_ = cfg().get_value("environment");
        if (!env_type_.compare("development"))
            env_type_ = "dev";
        else if (!env_type_.compare("testing"))
            env_type_ = "test";
        else if (!env_type_.compare("production"))
            env_type_ = "prod";
    }
    catch (const std::exception &e) {
        warning("Can't detect environment type, using 'test': "
                + std::string(e.what()));
        env_type_ = "test";
    }

    use_db_ = use_db;
    if (use_db_) {
        init_engine(cfg().get_value("DbBackend/@id"));
    }
}

IConfig &App::cfg()
{
    return *config_.get();
}

App::~App()
{
    engine_.reset(NULL);
    if (log_.get()) {
        info("log finished");
        FileLogAppender *appender = dynamic_cast<FileLogAppender *> (
                appender_.get());
        if (appender)
            appender->flush();
        if (file_stream_.get())
            file_stream_->close();
    }
    log_.reset(NULL);
    appender_.reset(NULL);
    file_stream_.reset(NULL);
}

Yb::Engine &App::get_engine()
{
    if (!engine_.get())
        throw RunTimeError("engine not created");
    return *engine_.get();
}

auto_ptr<Yb::Session> App::new_session()
{
    return auto_ptr<Yb::Session>(
            new Yb::Session(Yb::theSchema(), &get_engine()));
}

Yb::ILogger::Ptr App::new_logger(const string &name)
{
    if (!log_.get())
        throw RunTimeError("log not opened");
    return log_->new_logger(name);
}

Yb::ILogger::Ptr App::get_logger(const std::string &name)
{
    if (!log_.get())
        throw RunTimeError("log not opened");
    return log_->get_logger(name);
}

int App::get_level()
{
    if (!log_.get())
        throw RunTimeError("log not opened");
    return log_->get_level();
}

void App::set_level(int level)
{
    if (!log_.get())
        throw RunTimeError("log not opened");
    log_->set_level(level);
}

void App::log(int level, const string &msg)
{
    if (log_.get())
        log_->log(level, msg);
}

const string App::get_name() const
{
    if (!log_.get())
        return string();
    return log_->get_name();
}

// vim:ts=4:sts=4:sw=4:et:
