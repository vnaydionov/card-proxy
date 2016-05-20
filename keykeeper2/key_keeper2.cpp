#include <iostream>
#include <util/string_utils.h>
#include "app_class.h"
#include "micro_http.h"
#include "servant_utils.h"
#include "key_keeper_logic.h"

Yb::ElementTree::ElementPtr
ping(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    key_keeper->get();
    return key_keeper->mk_resp();
}

Yb::ElementTree::ElementPtr
read(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return key_keeper->read();
}

Yb::ElementTree::ElementPtr
get(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return key_keeper->get();
}

Yb::ElementTree::ElementPtr
write(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return key_keeper->write(params);
}

Yb::ElementTree::ElementPtr
set(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return key_keeper->set(params);
}

Yb::ElementTree::ElementPtr
unset(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return key_keeper->unset(params);
}

Yb::ElementTree::ElementPtr
cleanup(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return key_keeper->cleanup(params);
}


int main(int argc, char *argv[])
{
    auto config_file = Yb::StrUtils::xgetenv("CONFIG_FILE");
    if (!config_file.size())
        config_file = "/etc/card_proxy_keykeeper2/card_proxy_keykeeper2.cfg.xml";
    Yb::ILogger::Ptr logger;
    try {
        theApp::instance().init(
                IConfig::Ptr(new XmlConfig(config_file)),
                false);
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
        key_keeper = new KeyKeeper(theApp::instance().cfg(), *logger);
        const std::string prefix = "/" + theApp::instance().cfg()
            .get_value("HttpListener/Prefix");
        XmlHttpWrapper handlers_array[] = {
            WRAP("/", ping),
            WRAP(prefix, read),
            WRAP(prefix, get),
            WRAP(prefix, write),
            WRAP(prefix, set),
            WRAP(prefix, unset),
            WRAP(prefix, cleanup),
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
