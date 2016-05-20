// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LOGIC_INB_H
#define CARD_PROXY__LOGIC_INB_H

#include <util/nlogger.h>
#include "micro_http.h"

namespace LogicInbound {

const HttpResponse bind_card(Yb::ILogger &logger, const HttpRequest &request);

const HttpResponse supply_payment_data(Yb::ILogger &logger,
                                       const HttpRequest &request);

const HttpResponse start_payment(Yb::ILogger &logger, const HttpRequest &request);

} // LogicInbound

#endif // CARD_PROXY__LOGIC_INB_H
// vim:ts=4:sts=4:sw=4:et:
