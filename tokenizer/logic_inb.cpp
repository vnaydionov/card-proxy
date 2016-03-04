// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_inb.h"
#include "app_class.h"
#include "processors.h"
#include "proxy_any.h"
#include "json_object.h"
#include <util/string_utils.h>

#define CFG_VALUE(x) theApp::instance().cfg().get_value(x)

namespace LogicInbound {

const HttpMessage bind_card(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("ProxyUrl/bind_card_url"),
            "", "",
            bind_card__fix_json, NULL);
}

const HttpMessage supply_payment_data(Yb::ILogger &logger,
                                      const HttpMessage &request)
{
    using Yb::StrUtils::ends_with;
    auto uri = CFG_VALUE("ProxyUrl/bind_card_url");
    const std::string suffix = "/bind_card";
    YB_ASSERT(ends_with(uri, suffix));
    uri = uri.substr(0, uri.size() - suffix.size());
    uri += "/supply_payment_data";
    return proxy_any(
            logger, request, uri,
            "", "",
            bind_card__fix_json, NULL);
}

const HttpMessage start_payment(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("ProxyUrl/start_payment_url"),
            "", "",
            NULL, start_payment__fix_params);
}

} // LogicInbound

// vim:ts=4:sts=4:sw=4:et:
