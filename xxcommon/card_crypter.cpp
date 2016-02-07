// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "card_crypter.h"

CardCrypter::CardCrypter(IConfig &config, Yb::ILogger &logger,
                         Yb::Session &session)
    : tokenizer_(config, logger, session)
    , session_(session)
{}

CardData CardCrypter::get_token(const CardData &card_data)
{
    CardData result = card_data;
    result.clear_sensitive_data();
    if (!card_data.cvn.empty()) {
        Yb::DateTime cvn_finish_ts = Yb::dt_add_seconds(Yb::now(), 16 * 60);
        result.cvn_token = tokenizer_.tokenize(card_data.cvn,
                                               cvn_finish_ts, false);
    }
    Yb::DateTime exp_dt = card_data.expire_dt();
    std::string rnd = generate_random_bytes(4);
    Yb::DateTime card_finish_ts = Yb::dt_add_seconds(
            Yb::dt_make(
                Yb::dt_year(exp_dt),
                Yb::dt_month(exp_dt),
                1 + (((unsigned char)rnd[0]) * 28) / 256,
                (((unsigned char)rnd[1]) * 24) / 256,
                (((unsigned char)rnd[2]) * 60) / 256,
                (((unsigned char)rnd[3]) * 60) / 256),
            100 * 24 * 3600);
    result.card_token = tokenizer_.tokenize(
        card_data.pan, card_finish_ts, true);
    return result;
}

CardData CardCrypter::get_card(const std::string &card_token,
                               const std::string &cvn_token)
{
    CardData card_data(
            tokenizer_.detokenize(card_token));
    if (!cvn_token.empty())
        card_data.cvn = tokenizer_.detokenize(cvn_token);
    return card_data;
}

// vim:ts=4:sts=4:sw=4:et:
