#ifndef CRYPT_H
#define CRYPT_H
#include <string>


#include "utils.h"
#include "aes_crypter.h"
#include "card_data.h"
#include "domain/DataKey.h"
#include "domain/Card.h"
#include "domain/IncomingRequest.h"

std::string assemble_master_key();
std::string generate_token();
CardData generate_random_card_data();


class CardCrypter {
public:
    CardCrypter(Yb::Session &session);
    ~CardCrypter();

    Domain::Card get_token(const CardData &card_data);
    CardData get_card(const std::string &token);
    void remove_card(const std::string &token);
    void remove_card_data(const std::string &token);

    void change_master_key(const std::string &key);

private:
    Yb::Session &session;
    std::string _master_key;

    Domain::Card _save_card(AESCrypter &master_crypter, const CardData &card_data);
    Domain::IncomingRequest _save_cvn(AESCrypter &master_crypter, Domain::Card &card, const std::string &cvn);

    std::string _generate_card_token();

    std::string _get_encoded_str(AESCrypter &crypter, const std::string &str);
    std::string _get_decoded_str(AESCrypter &crypter, const std::string &str);
    std::string _get_decoded_dek(AESCrypter &crypter, const std::string &dek);
    std::string _get_encoded_dek(AESCrypter &crypter, const std::string &dek);
};

#endif
