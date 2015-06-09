#include "app_class.h"
#include "dek_pool.h"
#include <stdexcept>
#include <orm/domain_object.h>
#include <util/string_utils.h>

using namespace std;

void App::init_log(const string &log_name)
{
    if (!log_.get()) {
        file_stream_.reset(new ofstream(log_name.data(), ios::app));
        if (file_stream_->fail())
            throw runtime_error("can't open logfile: " + log_name);
        appender_.reset(new Yb::LogAppender(*file_stream_));
        log_.reset(new Yb::Logger(appender_.get()));
        info("log started");
    }
}

void App::init_engine(const string &db_name)
{
    if (!engine_.get()) {
        Yb::ILogger::Ptr yb_logger(new_logger("yb").release());
        Yb::init_schema();
        auto_ptr<Yb::SqlPool> pool(
                new Yb::SqlPool(YB_POOL_MAX_SIZE, YB_POOL_IDLE_TIME,
                    YB_POOL_MONITOR_SLEEP, yb_logger.get()));
        Yb::SqlSource src(cfg()->get_value("db_url"));
        src[_T("&id")] = cfg()->get_value("db_name");
        pool->add_source(src);
        engine_.reset(new Yb::Engine(Yb::Engine::READ_WRITE, pool, WIDEN(db_name)));
        engine_->set_echo(true);
        engine_->set_logger(yb_logger);

#if SCHEMA_DUMP
        Yb::theSchema().export_xml("schema_dump.xml");
        Yb::theSchema().export_ddl("schema_dump.sql", src.dialect());
#endif
#if SCHEMA_CREATE
        try {
            auto_ptr<Yb::EngineCloned> engine(engine_->clone());
            engine->create_schema(Yb::theSchema(), false);
            cerr << "Schema created\n";
        }
        catch (const Yb::DBError &e) {
            cerr << "Schema already exists\n";
        }
#endif
    }
}

//void App::init_dek_pool() {
//    session_ = new_session();
//    DEKPool::get_instance(*session_);
//}

void App::init(IConfig::Ptr config)
{
    config_.reset(config.release());
    init_log(cfg()->get_value("log_name"));
    init_engine(cfg()->get_value("db_name"));
    //init_dek_pool();
}

IConfig *App::cfg()
{
    return config_.get();
}

App::~App()
{
    engine_.reset(NULL);
    if (log_.get()) {
        info("log finished");
        appender_->flush();
        file_stream_->close();
    }
    log_.reset(NULL);
    appender_.reset(NULL);
    file_stream_.reset(NULL);
}

Yb::Engine *App::get_engine()
{
    if (!engine_.get())
        throw runtime_error("engine not created");
    return engine_.get();
}

auto_ptr<Yb::Session> App::new_session()
{
    return auto_ptr<Yb::Session>(
            new Yb::Session(Yb::theSchema(), get_engine()));
}

Yb::ILogger::Ptr App::new_logger(const string &name)
{
    if (!log_.get())
        throw runtime_error("log not open");
    return log_->new_logger(name);
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
