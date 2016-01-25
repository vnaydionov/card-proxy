#include <cpprest/http_client.h>
#include <util/string_utils.h>

#ifdef VAULT_DEBUG_API
#include "logic_debug.h"
#endif
#include "logic_token.h"
#include "servant_utils.h"
#include "listener_cb.h"


Yb::ElementTree::ElementPtr ping(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    if (params.get("exit", "") == "1")
        exit(1);
    session.engine()->exec_select("select 1 from dual", Yb::Values());
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    return resp;
}

#define CFG_VALUE(x) theApp::instance().cfg().get_value(x)

web::http::http_response bind_card(Yb::ILogger &logger,
                                   web::http::http_request &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("bind_card_url"),
            "", "",
            bind_card__fix_json, NULL);
}

web::http::http_response supply_payment_data(Yb::ILogger &logger,
                                             web::http::http_request &request)
{
    auto uri = CFG_VALUE("bind_card_url");
    uri = uri.substr(0, uri.size() - std::strlen("/bind_card"));
    uri += "/supply_payment_data";
    return proxy_any(
            logger, request, uri,
            "", "",
            bind_card__fix_json, NULL);
}

web::http::http_response authorize(Yb::ILogger &logger,
                                   web::http::http_request &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("authorize_url"),
            read_file(CFG_VALUE("authorize_url_cert")),
            read_file(CFG_VALUE("authorize_url_key")),
            NULL, authorize__fix_params);
}

web::http::http_response start_payment(Yb::ILogger &logger,
                                       web::http::http_request &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("start_payment_url"),
            "", "",
            NULL, start_payment__fix_params);
}

web::http::http_response proxy_host2host_ym_api(
        Yb::ILogger &logger, web::http::http_request &request,
        const std::string &method)
{
    auto host2host_uri = CFG_VALUE("authorize_url");
    host2host_uri = host2host_uri.substr(0, host2host_uri.size()
                                         - std::strlen("/authorize"));
    host2host_uri += "/" + method;
    auto host2host_cert = read_file(CFG_VALUE("authorize_url_cert"));
    auto host2host_key = read_file(CFG_VALUE("authorize_url_key"));
    return proxy_any(
            logger, request, host2host_uri,
            host2host_cert, host2host_key,
            NULL, NULL);
}

web::http::http_response status(Yb::ILogger &logger,
                                web::http::http_request &request)
{
    return proxy_host2host_ym_api(logger, request, "status");
}

web::http::http_response cancel(Yb::ILogger &logger,
                                web::http::http_request &request)
{
    return proxy_host2host_ym_api(logger, request, "cancel");
}

web::http::http_response clear(Yb::ILogger &logger,
                               web::http::http_request &request)
{
    return proxy_host2host_ym_api(logger, request, "clear");
}


int main(int argc, char *argv[])
{
    const std::string dbg_prefix = "/debug_api/";
    const std::string ping_prefix = "/";
    const std::string inbound_prefix = "/cp/inbound/";
    const std::string outbound_prefix = "/cp/outbound/";
    CardProxyHttpWrapper handlers[] = {
        // service methods
        CWRAP(ping_prefix, ping),
#ifdef VAULT_DEBUG_API
        // debug methods
        CWRAP(dbg_prefix, debug_method),
        CWRAP(dbg_prefix, dek_status),
        CWRAP(dbg_prefix, get_token),
        CWRAP(dbg_prefix, get_card),
        CWRAP(dbg_prefix, remove_card),
        CWRAP(dbg_prefix, remove_incoming_request),
        CWRAP(dbg_prefix, set_master_key),
        CWRAP(dbg_prefix, get_master_key),
        CWRAP(dbg_prefix, run_load_scenario),
#endif
        // proxy methods
        CWRAP(inbound_prefix, bind_card),
        CWRAP(inbound_prefix, supply_payment_data),
        CWRAP(inbound_prefix, start_payment),
        CWRAP(outbound_prefix, authorize),
        // a temporary proxy methods of YM host2host API
        CWRAP(outbound_prefix, status),
        CWRAP(outbound_prefix, cancel),
        CWRAP(outbound_prefix, clear),
    };
    int n_handlers = sizeof(handlers)/sizeof(handlers[0]);
    auto config_file = Yb::StrUtils::xgetenv("CONFIG_FILE");
    if (!config_file.size())
        config_file = "/etc/card_proxy/card_proxy.cfg.xml";
    return run_server_app(config_file, handlers, n_handlers);
}

// vim:ts=4:sts=4:sw=4:et:
