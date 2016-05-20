#include <iostream>
#include "app_class.h"
#include "micro_http.h"
#include "servant_utils.h"
#include "keyapi_logic.h"
#include <util/string_utils.h>

#include "domain/VaultUser.h"

Yb::ElementTree::ElementPtr
ping(Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    return KeyAPI(theApp::instance().cfg(), logger, session).mk_resp();
}

#define KEYAPI_METHOD(method) \
Yb::ElementTree::ElementPtr \
    method(Yb::Session &session, Yb::ILogger &logger, \
        const Yb::StringDict &params) \
{ \
    return KeyAPI(theApp::instance().cfg(), \
                  logger, session).method(params); \
}

KEYAPI_METHOD(generate_hmac)
KEYAPI_METHOD(generate_kek)
KEYAPI_METHOD(get_component)
KEYAPI_METHOD(confirm_component)
KEYAPI_METHOD(reset_target_version)
KEYAPI_METHOD(cleanup)
KEYAPI_METHOD(rehash_tokens)
KEYAPI_METHOD(reencrypt_deks)
KEYAPI_METHOD(switch_hmac)
KEYAPI_METHOD(switch_kek)
KEYAPI_METHOD(status)


int main(int argc, char *argv[])
{
    //just to trigger linking
    Domain::VaultUser dummy;

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
            WRAP(prefix, generate_hmac),
            WRAP(prefix, generate_kek),
            WRAP(prefix, get_component),
            WRAP(prefix, confirm_component),
            WRAP(prefix, reset_target_version),
            WRAP(prefix, cleanup),
            WRAP(prefix, rehash_tokens),
            WRAP(prefix, reencrypt_deks),
            WRAP(prefix, switch_hmac),
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
