// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_outb.h"
#include "app_class.h"
#include "processors.h"
#include "proxy_any.h"
#include <util/string_utils.h>

#define CFG_VALUE(x) theApp::instance().cfg().get_value(x)

namespace LogicOutbound {

const HttpResponse authorize(Yb::ILogger &logger, const HttpRequest &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("ProxyUrl/authorize_url"),
            CFG_VALUE("ProxyUrl/authorize_url_cert"),
            CFG_VALUE("ProxyUrl/authorize_url_key"),
            NULL, authorize__fix_params);
}

const HttpResponse proxy_processing_api(Yb::ILogger &logger,
                                        const HttpRequest &request,
                                        const std::string &method)
{
    using Yb::StrUtils::ends_with;
    auto uri = CFG_VALUE("ProxyUrl/authorize_url");
    const std::string suffix = "/authorize";
    YB_ASSERT(ends_with(uri, suffix));
    uri = uri.substr(0, uri.size() - suffix.size());
    uri += "/" + method;
    auto processing_cert = CFG_VALUE("ProxyUrl/authorize_url_cert");
    auto processing_key = CFG_VALUE("ProxyUrl/authorize_url_key");
    return proxy_any(
            logger, request, uri,
            processing_cert, processing_key,
            NULL, NULL);
}

const HttpResponse status(Yb::ILogger &logger, const HttpRequest &request)
{
    return proxy_processing_api(logger, request, "status");
}

const HttpResponse cancel(Yb::ILogger &logger, const HttpRequest &request)
{
    return proxy_processing_api(logger, request, "cancel");
}

const HttpResponse clear(Yb::ILogger &logger, const HttpRequest &request)
{
    return proxy_processing_api(logger, request, "clear");
}

} // LogicOutbound

// vim:ts=4:sts=4:sw=4:et:
