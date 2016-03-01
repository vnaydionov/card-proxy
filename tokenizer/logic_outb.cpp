// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_outb.h"
#include "app_class.h"
#include "processors.h"
#include "proxy_any.h"

#define CFG_VALUE(x) theApp::instance().cfg().get_value(x)

namespace LogicOutbound {

const HttpMessage authorize(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("authorize_url"),
            CFG_VALUE("authorize_url_cert"),
            CFG_VALUE("authorize_url_key"),
            NULL, authorize__fix_params);
}

const HttpMessage proxy_host2host_ym_api(Yb::ILogger &logger,
                                         const HttpMessage &request,
                                         const std::string &method)
{
    auto host2host_uri = CFG_VALUE("authorize_url");
    host2host_uri = host2host_uri.substr(0, host2host_uri.size()
                                         - std::strlen("/authorize"));
    host2host_uri += "/" + method;
    auto host2host_cert = CFG_VALUE("authorize_url_cert");
    auto host2host_key = CFG_VALUE("authorize_url_key");
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

} // LogicOutbound

// vim:ts=4:sts=4:sw=4:et:
