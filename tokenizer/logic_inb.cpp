// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_inb.h"
#include "app_class.h"
#include "processors.h"
#include "proxy_any.h"

#define CFG_VALUE(x) theApp::instance().cfg().get_value(x)

namespace LogicInbound {

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

const HttpMessage start_payment(Yb::ILogger &logger, const HttpMessage &request)
{
    return proxy_any(
            logger, request, CFG_VALUE("start_payment_url"),
            "", "",
            NULL, start_payment__fix_params);
}

} // LogicInbound

// vim:ts=4:sts=4:sw=4:et:
