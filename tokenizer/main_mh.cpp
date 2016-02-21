// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <cstring>
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

Yb::ElementTree::ElementPtr check_kek(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
#ifdef TOKENIZER_CONFIG_SINGLETON
    TokenizerConfig &tcfg(theTokenizerConfig::instance().refresh());
#else
    TokenizerConfig tcfg;
#endif
    std::string ver = params.get("kek_version", "");
    int kek_version = -1;
    if (ver.empty())
        kek_version = tcfg.get_active_master_key_version();
    else
        kek_version = boost::lexical_cast<int>(ver);
    resp->sub_element("check_kek",
            tcfg.is_kek_valid(kek_version)? "true": "false");
    return resp;
}

#define CFG_VALUE(x) theApp::instance().cfg().get_value(x)

const HttpMessage bind_card(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("bind_card_url"),
            "", "",
            bind_card__fix_json, NULL);
}

const HttpMessage supply_payment_data(Yb::ILogger &logger,
                                      const HttpMessage &request)
{
    auto uri = CFG_VALUE("bind_card_url");
    uri = uri.substr(0, uri.size() - std::strlen("/bind_card"));
    uri += "/supply_payment_data";
    return proxy_any(
            logger, request, uri,
            "", "",
            bind_card__fix_json, NULL);
}

const HttpMessage authorize(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("authorize_url"),
            read_file(CFG_VALUE("authorize_url_cert")),
            read_file(CFG_VALUE("authorize_url_key")),
            NULL, authorize__fix_params);
}

const HttpMessage start_payment(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("start_payment_url"),
            "", "",
            NULL, start_payment__fix_params);
}

const HttpMessage proxy_host2host_ym_api(Yb::ILogger &logger,
                                         const HttpMessage &request,
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

const HttpMessage status(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_host2host_ym_api(logger, request, "status");
}

const HttpMessage cancel(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_host2host_ym_api(logger, request, "cancel");
}

const HttpMessage clear(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_host2host_ym_api(logger, request, "clear");
}


int main(int argc, char *argv[])
{
    const std::string dbg_prefix = "/debug_api/";
    const std::string ping_prefix = "/service/";
    const std::string inbound_prefix = "/cp/inbound/";
    const std::string outbound_prefix = "/cp/outbound/";
    CardProxyHttpWrapper handlers[] = {
        // service methods
        WRAP(ping_prefix, ping),
        WRAP(ping_prefix, check_kek),
#ifdef VAULT_DEBUG_API
        // debug methods
        WRAP(dbg_prefix, debug_method),
        WRAP(dbg_prefix, dek_status),
        WRAP(dbg_prefix, tokenize_card),
        WRAP(dbg_prefix, detokenize_card),
        WRAP(dbg_prefix, remove_card),
        WRAP(dbg_prefix, run_load_scenario),
#endif
        // proxy methods
        WRAP(inbound_prefix, bind_card),
        WRAP(inbound_prefix, supply_payment_data),
        WRAP(inbound_prefix, start_payment),
        WRAP(outbound_prefix, authorize),
        // a temporary proxy methods of YM host2host API
        WRAP(outbound_prefix, status),
        WRAP(outbound_prefix, cancel),
        WRAP(outbound_prefix, clear),
    };
    int n_handlers = sizeof(handlers)/sizeof(handlers[0]);
    auto config_file = Yb::StrUtils::xgetenv("CONFIG_FILE");
    if (!config_file.size())
        config_file = "/etc/card_proxy_tokenizer/card_proxy_tokenizer.cfg.xml";
    return run_server_app(config_file, handlers, n_handlers);
}

// vim:ts=4:sts=4:sw=4:et:
