// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "card_crypter.h"

const std::string normalize_pan(const std::string &pan)
{
    std::string r;
    r.reserve(pan.size());
    for (size_t i = 0; i < pan.size(); ++i) {
        const unsigned char c = (unsigned char )pan[i];
        if (c >= '0' && c <= '9')
            r += c;
        else if (!std::strchr(" \t\n\r", c))
            throw RunTimeError("Wrong character in PAN");
    }
    if (r.size() < 12 || r.size() > 19)
        throw RunTimeError("Invalid PAN length: " + Yb::to_string(pan.size()));
    return r;
}

const std::string mask_pan(const std::string &pan)
{
    std::string norm_pan = normalize_pan(pan);
    if (norm_pan.size() < 16)
        return pan.substr(0, 2) + "****" + pan.substr(pan.size() - 4);
    else
        return pan.substr(0, 6) + "****" + pan.substr(pan.size() - 4);
}

int normalize_year(int year)
{
    if (year && year < 2000)
        return 2000 + year;
    return year;
}

char luhn_control_digit(const char *num, size_t len)
{
    const char *ptr = num + len - 1, *end = num - 1;
    int i = 0, sum = 0;
    for (; ptr != end; --ptr, ++i)
    {
        int p = *ptr - '0';
        sum += !(i % 2)? (p*2 > 9? p*2 - 9: p*2): p;
    }
    sum = (10 - (sum % 10)) % 10;
    return sum + '0';
}

bool luhn_check(const char *num, size_t len)
{
    const char *ptr = num + len - 1, *end = num - 1;
    int i = 0, sum = 0;
    for (; ptr != end; --ptr, ++i)
    {
        int p = *ptr - '0';
        sum += i % 2? (p*2 > 9? p*2 - 9: p*2): p;
    }
    return sum % 10 == 0;
}

const std::string generate_pan(int pan_len)
{
    const char prefix[] = "500000";  // not to mess with real world cards
    const size_t prefix_len = std::strlen(prefix);
    const size_t gen_len = pan_len - prefix_len - 1;
    YB_ASSERT(gen_len >= 1);
    std::string pan(pan_len, '0');
    std::memcpy(&pan[0], prefix, prefix_len);
    std::memcpy(&pan[prefix_len], generate_random_number(gen_len).c_str(),
                gen_len);
    pan[pan_len - 1] = luhn_control_digit(&pan[0], pan_len - 1);
    return pan;
}

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
