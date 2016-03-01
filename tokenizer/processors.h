// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__PROCESSORS_H
#define CARD_PROXY__PROCESSORS_H

#include <string>
#include <util/data_types.h>
#include <util/nlogger.h>

typedef const std::string (*BodyProcessor)(Yb::ILogger &,
                                           const std::string &);

typedef const Yb::StringDict (*ParamsProcessor)(Yb::ILogger &,
                                                const Yb::StringDict &);

const std::string bind_card__fix_json(Yb::ILogger &logger,
                                      const std::string &body);

const Yb::StringDict authorize__fix_params(Yb::ILogger &logger,
                                           const Yb::StringDict &params0);

const Yb::StringDict start_payment__fix_params(Yb::ILogger &logger,
                                               const Yb::StringDict &params0);

#endif // CARD_PROXY__PROCESSORS_H
// vim:ts=4:sts=4:sw=4:et:
