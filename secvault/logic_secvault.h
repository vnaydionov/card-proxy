// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LOGIC_SECVAULT_H
#define CARD_PROXY__LOGIC_SECVAULT_H

#include <util/data_types.h>
#include <util/element_tree.h>
#include <util/nlogger.h>
#include <orm/data_object.h>

#include "tokenizer.h"

#define DECL_SECVAULT_METHOD(name) \
Yb::ElementTree::ElementPtr name( \
        Yb::Session &session, Yb::ILogger &logger, \
        const Yb::StringDict &params)

namespace LogicSecVault {

DECL_SECVAULT_METHOD(tokenize);
DECL_SECVAULT_METHOD(detokenize);
DECL_SECVAULT_METHOD(search_token);
DECL_SECVAULT_METHOD(remove_token);

} // LogicSecVault

#endif // CARD_PROXY__LOGIC_SECVAULT_H
// vim:ts=4:sts=4:sw=4:et:
