// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <cstring>
#include <pplx/pplx.h>
#include <cpprest/http_client.h>
#include <util/string_utils.h>

#ifdef VAULT_DEBUG_API
#include "logic_debug.h"
#endif
#include "logic_token.h"
#include "servant_utils.h"
#include "listener_mh.h"


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

const HttpHeaders bind_card(Yb::ILogger &logger, const HttpHeaders &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("bind_card_url"),
            "", "",
            bind_card__fix_json, NULL);
}

const HttpHeaders supply_payment_data(Yb::ILogger &logger,
                                      const HttpHeaders &request)
{
    auto uri = CFG_VALUE("bind_card_url");
    uri = uri.substr(0, uri.size() - std::strlen("/bind_card"));
    uri += "/supply_payment_data";
    return proxy_any(
            logger, request, uri,
            "", "",
            bind_card__fix_json, NULL);
}

const HttpHeaders authorize(Yb::ILogger &logger, const HttpHeaders &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("authorize_url"),
            read_file(CFG_VALUE("authorize_url_cert")),
            read_file(CFG_VALUE("authorize_url_key")),
            NULL, authorize__fix_params);
}

const HttpHeaders start_payment(Yb::ILogger &logger, const HttpHeaders &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("start_payment_url"),
            "", "",
            NULL, start_payment__fix_params);
}

const HttpHeaders proxy_host2host_ym_api(Yb::ILogger &logger,
                                         const HttpHeaders &request,
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

const HttpHeaders status(Yb::ILogger &logger, const HttpHeaders &request)
{
    return proxy_host2host_ym_api(logger, request, "status");
}

const HttpHeaders cancel(Yb::ILogger &logger, const HttpHeaders &request)
{
    return proxy_host2host_ym_api(logger, request, "cancel");
}

const HttpHeaders clear(Yb::ILogger &logger, const HttpHeaders &request)
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
        WRAP(ping, ping_prefix),
#ifdef VAULT_DEBUG_API
        // debug methods
        WRAP(debug_method, dbg_prefix),
        WRAP(dek_status, dbg_prefix),
        WRAP(get_token, dbg_prefix),
        WRAP(get_card, dbg_prefix),
        WRAP(remove_card, dbg_prefix),
        WRAP(remove_incoming_request, dbg_prefix),
        WRAP(set_master_key, dbg_prefix),
        WRAP(get_master_key, dbg_prefix),
        WRAP(run_load_scenario, dbg_prefix),
#endif
        // proxy methods
        WRAP(bind_card, inbound_prefix),
        WRAP(supply_payment_data, inbound_prefix),
        WRAP(start_payment, inbound_prefix),
        WRAP(authorize, outbound_prefix),
        // a temporary proxy methods of YM host2host API
        WRAP(status, outbound_prefix),
        WRAP(cancel, outbound_prefix),
        WRAP(clear, outbound_prefix),
    };
    int n_handlers = sizeof(handlers)/sizeof(handlers[0]);
    auto config_file = Yb::StrUtils::xgetenv("CONFIG_FILE");
    if (!config_file.size())
        config_file = "/etc/card_proxy/card_proxy.cfg.xml";
    return run_server_app(config_file, handlers, n_handlers);
}

// vim:ts=4:sts=4:sw=4:et:
