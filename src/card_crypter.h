// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__CARD_CRYPTER_H
#define CARD_PROXY__CARD_CRYPTER_H

#include <string>
#include <util/data_types.h>
#include <orm/data_object.h>

#include "utils.h"
#include "conf_reader.h"
#include "dek_pool.h"

#include "domain/Card.h"
#include "domain/IncomingRequest.h"

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

    CardData(const std::string &_pan,
             int _expire_year, int _expire_month,
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
        return std::to_string(expire_year);
    }

    const std::string format_month() const {
        std::string s = std::to_string(expire_month);
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

    bool is_fake_card_trust() const {
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
    CardCrypter(IConfig &config, Yb::ILogger &logger, Yb::Session &session)
        : config_(config)
        , logger_(logger.new_logger("card_crypter").release())
        , session_(session)
        , master_key_(assemble_master_key(config, *logger_, session))
        , dek_pool_(config, *logger_, session, master_key_)
    {}

    // incoming request processing
    CardData get_token(const CardData &card_data);

    // outgoing request processing
    CardData get_card(const std::string &card_token,
                      const std::string &cvn_token = "");

    int remove_card(const std::string &card_token);
    int remove_incoming_request(const std::string &cvn_token);
    void change_master_key(const std::string &key);

    const std::string generate_data_token();
    const std::string generate_card_token();
    const std::string generate_cvn_token();

    static const std::string assemble_master_key(IConfig &config,
                                                 Yb::ILogger &logger,
                                                 Yb::Session &session);
    static const std::string encode_data(const std::string &dek,
                                         const std::string &data);
    static const std::string decode_data(const std::string &dek,
                                         const std::string &data_crypted);

    const std::string encode_dek(const std::string &dek) {
        return encode_data(master_key_, dek);
    }

    const std::string decode_dek(const std::string &dek_crypted) {
        return decode_data(master_key_, dek_crypted);
    }

private:
    IConfig &config_;
    Yb::ILogger::Ptr logger_;
    Yb::Session &session_;
    std::string master_key_;
    DEKPool dek_pool_;

    Domain::Card save_card(const CardData &card_data);
    Domain::IncomingRequest save_cvn(const CardData &card_data);

    static const std::string recv_key_from_server(IConfig &config);
    static void send_key_to_server(IConfig &config, const std::string &key);
};

class TokenNotFound: public RunTimeError
{
public:
    TokenNotFound(): RunTimeError("Token not found") {}
};

#endif // CARD_PROXY__CARD_CRYPTER_H
// vim:ts=4:sts=4:sw=4:et:
