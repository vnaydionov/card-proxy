// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__CARD_CRYPTER_H
#define CARD_PROXY__CARD_CRYPTER_H

#include "tokenizer.h"

std::string mask_pan(const std::string &pan);
std::string normalize_pan(const std::string &pan);
int normalize_year(int year);
char luhn_control_digit(const char *num, size_t len);
bool luhn_check(const char *num, size_t len);
const std::string generate_pan(int pan_len = 16);


/* The following fields are recognized:
 * pan, expire_year, expire_month, card_holder, cvn.
 *
 * These are added after tokenization:
 * pan_masked, card_token, cvn_token
 */
struct CardData
{
    CardData()
        : expire_year(0)
        , expire_month(0)
    {}

    explicit CardData(const std::string &_pan,
             int _expire_year = 0, int _expire_month = 0,
             const std::string &_card_holder = "",
             const std::string &_cvn = "")
        : expire_year(normalize_year(_expire_year))
        , expire_month(_expire_month)
        , pan(normalize_pan(_pan))
        , card_holder(_card_holder)
        , cvn(_cvn)
        , pan_masked(mask_pan(pan))
    {}

    bool empty() const { return !expire_year && !expire_month; }

    const std::string format_year() const {
        return Yb::to_string(expire_year);
    }

    const std::string format_month() const {
        std::string s = Yb::to_string(expire_month);
        if (s.size() == 1)
            s = "0" + s;
        return s;
    }

    const Yb::DateTime expire_dt() const {
        int xyear = expire_year;
        int xmonth = expire_month + 1;
        if (xmonth > 12) {
            ++xyear;
            xmonth = 1;
        }
        return Yb::dt_make(xyear, xmonth, 1);
    }

    void set_expire_dt(const Yb::DateTime &expire_dt) {
        int xyear = Yb::dt_year(expire_dt);
        int xmonth = Yb::dt_month(expire_dt) - 1;
        if (xmonth < 1) {
            --xyear;
            xmonth = 12;
        }
        expire_year = xyear;
        expire_month = xmonth;
    }

    bool is_fake_card() const {
        return (
            pan == "5555555555554444" ||
            pan == "5105105105105100" ||
            pan == "4111111111111111" ||
            pan == "4012888888881881"
        );
    }

    void clear_sensitive_data() {
        pan.clear();
        cvn.clear();
    }

    // public fields:
    int expire_year, expire_month;
    std::string pan, card_holder, cvn, pan_masked, card_token, cvn_token;
};


class CardCrypter
{
public:
    CardCrypter(IConfig &config, Yb::ILogger &logger, Yb::Session &session);

    // incoming request processing
    CardData get_token(const CardData &card_data);

    // outgoing request processing
    CardData get_card(const std::string &card_token,
                      const std::string &cvn_token = "");

    const std::string search(const std::string &token)
    {
        return tokenizer_.search(token);
    }

    int remove_data_token(const std::string &token_string)
    {
        return tokenizer_.remove_data_token(token_string);
    }

    const std::string generate_token_string()
    {
        return tokenizer_.generate_token_string();
    }

    static const std::string encrypt_data(const std::string &dek,
                                         const std::string &data)
    {
        return Tokenizer::encrypt_data(dek, data);
    }
    static const std::string decrypt_data(const std::string &dek,
                                         const std::string &data_crypted)
    {
        return Tokenizer::decrypt_data(dek, data_crypted);
    }

    const std::string encrypt_dek(const std::string &dek,
                                 int kek_version)
    {
        return tokenizer_.encrypt_dek(dek, kek_version);
    }
    const std::string decrypt_dek(const std::string &dek_crypted,
                                 int kek_version)
    {
        return tokenizer_.decrypt_dek(dek_crypted, kek_version);
    }

private:
    Tokenizer tokenizer_;
    Yb::Session &session_;
};

#endif // CARD_PROXY__CARD_CRYPTER_H
// vim:ts=4:sts=4:sw=4:et:
