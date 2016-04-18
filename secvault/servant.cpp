// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_service.h"
#include "logic_secvault.h"
#include "servant_utils.h"
#include "app_class.h"
#include "micro_http.h"

typedef XmlHttpWrapper CardProxyHttpWrapper;

template <class HttpHandler>
inline int run_server_app(const std::string &config_name,
        HttpHandler *handlers_array, int n_handlers)
{
    randomize();
    Yb::ILogger::Ptr logger;
    try {
        theApp::instance().init(IConfig::Ptr(new XmlConfig(config_name)));
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
        typedef HttpServer<HttpHandler> MyHttpServer;
        typename MyHttpServer::HandlerMap handlers;
        for (int i = 0; i < n_handlers; ++i) {
            std::string prefix = handlers_array[i].prefix();
            handlers[prefix + handlers_array[i].name()] = handlers_array[i];
        }
        std::string error_content_type = "text/json";
        std::string error_body = "{\"status\": \"internal_error\"}";
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

int main(int argc, char *argv[])
{
    const std::string servault_prefix = "/secvault/";
    const std::string ping_prefix = "/service/";
    using namespace LogicService;
    using namespace LogicSecVault;
    CardProxyHttpWrapper handlers[] = {
        // service methods
        WRAP(ping_prefix, ping),
        WRAP(ping_prefix, check_kek),
        // secvault methods
        WRAP(servault_prefix, tokenize),
        WRAP(servault_prefix, detokenize),
        WRAP(servault_prefix, search_token),
        WRAP(servault_prefix, remove_token),
    };
    int n_handlers = sizeof(handlers)/sizeof(handlers[0]);
    auto config_file = Yb::StrUtils::xgetenv("CONFIG_FILE");
    if (!config_file.size())
        config_file = "/etc/card_proxy_secvault/card_proxy_secvault.cfg.xml";
    return run_server_app(config_file, handlers, n_handlers);
}

// vim:ts=4:sts=4:sw=4:et:
