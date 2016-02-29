#include <iostream>
#include "app_class.h"
#include "micro_http.h"
#include "servant_utils.h"
#include "keyapi_logic.h"

Yb::ElementTree::ElementPtr
ping(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(), logger, session).mk_resp();
}

Yb::ElementTree::ElementPtr
generate_kek(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).generate_kek(params);
}

Yb::ElementTree::ElementPtr
get_component(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).get_component(params);
}

Yb::ElementTree::ElementPtr
confirm_component(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).confirm_component(params);
}

Yb::ElementTree::ElementPtr
reset_target_version(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).reset_target_version(params);
}

Yb::ElementTree::ElementPtr
cleanup(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).cleanup(params);
}

Yb::ElementTree::ElementPtr
reencrypt_deks(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).reencrypt_deks(params);
}

Yb::ElementTree::ElementPtr
switch_kek(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).switch_kek(params);
}

Yb::ElementTree::ElementPtr
status(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(),
                  logger, session).status(params);
}


int main(int argc, char *argv[])
{
    auto config_file = Yb::StrUtils::xgetenv("CONFIG_FILE");
    if (!config_file.size())
        config_file = "/etc/card_proxy_keyapi/card_proxy_keyapi.cfg.xml";
    randomize();
    Yb::ILogger::Ptr logger;
    try {
        theApp::instance().init(
                IConfig::Ptr(new XmlConfig(config_file)));
        logger.reset(theApp::instance().new_logger("main").release());
    }
    catch (const std::exception &ex) {
        std::cerr << "exception: " << ex.what() << "\n";
        return 1;
    }
    try {
        std::string bind_host = theApp::instance().cfg()
            .get_value("HttpListener/Host");
        int bind_port = theApp::instance().cfg()
            .get_value_as_int("HttpListener/Port");
        std::string listen_at = "http://" + bind_host + ":"
                + Yb::to_string(bind_port) + "/";
        logger->error("listen at: " + listen_at);
        const std::string prefix = "/" + theApp::instance().cfg()
            .get_value("HttpListener/Prefix");
        XmlHttpWrapper handlers_array[] = {
            WRAP("/", ping),
            WRAP(prefix, generate_kek),
            WRAP(prefix, get_component),
            WRAP(prefix, confirm_component),
            WRAP(prefix, reset_target_version),
            WRAP(prefix, cleanup),
            WRAP(prefix, reencrypt_deks),
            WRAP(prefix, switch_kek),
            WRAP(prefix, status),
        };
        int n_handlers = sizeof(handlers_array)/sizeof(handlers_array[0]);
        typedef HttpServer<XmlHttpWrapper> MyHttpServer;
        MyHttpServer::HandlerMap handlers;
        for (int i = 0; i < n_handlers; ++i) {
            std::string prefix = handlers_array[i].prefix();
            handlers[prefix + handlers_array[i].name()] = handlers_array[i];
        }
        std::string error_content_type = "application/xml";
        std::string error_body = "<result><status>internal_error</status></result>";
        MyHttpServer server(
                bind_host, bind_port, 30, handlers, &theApp::instance(),
                error_content_type, error_body);
        server.serve();
    }
    catch (const std::exception &ex) {
        logger->error(std::string("exception: ") + ex.what());
        return 1;
    }
    return 0;
}

// vim:ts=4:sts=4:sw=4:et:
