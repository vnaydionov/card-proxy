// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LOGIC_OUTB_H
#define CARD_PROXY__LOGIC_OUTB_H

#include <util/nlogger.h>
#include "micro_http.h"

namespace LogicOutbound {

const HttpResponse authorize(Yb::ILogger &logger, const HttpRequest &request);

const HttpResponse status(Yb::ILogger &logger, const HttpRequest &request);

const HttpResponse cancel(Yb::ILogger &logger, const HttpRequest &request);

const HttpResponse clear(Yb::ILogger &logger, const HttpRequest &request);

} // LogicOutbound

#endif // CARD_PROXY__LOGIC_OUTB_H
// vim:ts=4:sts=4:sw=4:et:
