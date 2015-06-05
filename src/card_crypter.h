// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__CARD_CRYPTER_H
#define CARD_PROXY__CARD_CRYPTER_H

#include <string>
#include <sys/socket.h>
#include <orm/data_object.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

    void remove_card(const std::string &token);
    void remove_card_data(const std::string &token);
    void change_master_key(const std::string &key);

    static const std::string assemble_master_key(Yb::Session &session);
    static const std::string generate_card_token();
    static const std::string encode_data(const std::string &dek,
                                         const std::string &data);
    static const std::string decode_data(const std::string &dek,
                                         const std::string &data_crypted);

    const std::string encode_dek(const std::string &dek)
    {
        return encode_data(master_key_, dek);
    }

    const std::string decode_dek(const std::string &dek_crypted)
    {
        return decode_data(master_key_, dek_crypted);
    }

private:
    Yb::Session &session_;
    std::string master_key_;
};

class TokenNotFound: public std::runtime_error
{
    TokenNotFound():
        std::runtime_error("Token not found")
    {}
};

class CardNotFound: public std::runtime_error
{
    CardNotFound():
        std::runtime_error("Card not found")
    {}
};

Yb::DateTime mk_expire_dt(int expire_year, int expire_month);
std::pair<int, int> split_expire_dt(const Yb::DateTime &expire_dt);

#endif // CARD_PROXY__CARD_CRYPTER_H
// vim:ts=4:sts=4:sw=4:et:
