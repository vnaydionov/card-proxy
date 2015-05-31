// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__CRYPT_H
#define CARD_PROXY__CRYPT_H

#include <string>
#include <orm/data_object.h>

/* The following fields are recognized:
 * pan, expire_year, expire_month, card_holder, cvn.
 *
 * These are added after tokenization:
 * pan_masked, card_token
 */
typedef Yb::StringDict CardData;

class CardCrypter
{
public:
    CardCrypter(Yb::Session &session)
        : session_(session)
        , master_key_(assemble_master_key(session))
    {}

    Yb::Session &session() { return session_; }

    // incoming request processing
    CardData get_token(const CardData &card_data);

    // outgoing request processing
    CardData get_card(const std::string &token);

    static std::string assemble_master_key(Yb::Session &session);
    static std::string generate_card_token();
    static std::string encode_data(const std::string &dek,
                                   const std::string &data);
    static std::string decode_data(const std::string &dek,
                                   const std::string &dek_crypted);

    std::string encode_dek(const std::string &dek)
    {
        return encode_data(master_key_, dek);
    }

    std::string decode_dek(const std::string &dek_crypted)
    {
        return decode_data(master_key_, dek_crypted);
    }

private:
    Yb::Session &session_;
    std::string master_key_;
};

#endif // CARD_PROXY__CRYPT_H
// vim:ts=4:sts=4:sw=4:et:
