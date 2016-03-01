// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LOGIC_OUTB_H
#define CARD_PROXY__LOGIC_OUTB_H

#include <util/nlogger.h>
#include "micro_http.h"

namespace LogicOutbound {

const HttpMessage authorize(Yb::ILogger &logger, const HttpMessage &request);

const HttpMessage status(Yb::ILogger &logger, const HttpMessage &request);

const HttpMessage cancel(Yb::ILogger &logger, const HttpMessage &request);

const HttpMessage clear(Yb::ILogger &logger, const HttpMessage &request);

} // LogicOutbound

#endif // CARD_PROXY__LOGIC_OUTB_H
// vim:ts=4:sts=4:sw=4:et:
