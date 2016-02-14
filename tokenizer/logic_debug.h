// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__LOGIC_DEBUG_H
#define CARD_PROXY__LOGIC_DEBUG_H

#include <util/data_types.h>
#include <util/element_tree.h>
#include <util/nlogger.h>
#include <orm/data_object.h>

#include "card_crypter.h"

char luhn_control_digit(const std::string &num);
bool luhn_check(const std::string &num);
CardData generate_random_card_data();

void write_card_data_to_xml(const CardData &card_data,
                            Yb::ElementTree::ElementPtr cd);

#define DECL_DEBG_METHOD(name) \
Yb::ElementTree::ElementPtr name( \
        Yb::Session &session, Yb::ILogger &logger, \
        const Yb::StringDict &params)

DECL_DEBG_METHOD(debug_method);
DECL_DEBG_METHOD(dek_status);
DECL_DEBG_METHOD(tokenize_card);
DECL_DEBG_METHOD(detokenize_card);
DECL_DEBG_METHOD(remove_card);
DECL_DEBG_METHOD(run_load_scenario);

#endif // CARD_PROXY__LOGIC_DEBUG_H
// vim:ts=4:sts=4:sw=4:et:
