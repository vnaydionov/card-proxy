// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LOGIC_SERVICE_H
#define CARD_PROXY__LOGIC_SERVICE_H

#include <util/data_types.h>
#include <util/element_tree.h>
#include <util/nlogger.h>
#include <orm/data_object.h>

namespace LogicService {

Yb::ElementTree::ElementPtr ping(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params);

Yb::ElementTree::ElementPtr check_kek(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params);

} // LogicService

#endif // CARD_PROXY__LOGIC_SERVICE_H
// vim:ts=4:sts=4:sw=4:et:
