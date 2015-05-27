#ifndef CRYPT_H
#define CRYPT_H
#include <string>

#include <orm/data_object.h>
#include <orm/domain_object.h>

#include "utils.h"
#include "aes_crypter.h"
#include "card_data.h"
#include "domain/DataKey.h"
#include "domain/Card.h"


struct DEKPoolStatus {
    long total_count;
    long active_count;
    long use_count;
};

std::string assemble_master_key();

std::string generate_token();
Domain::DataKey get_active_data_key(Yb::Session &session);
Domain::DataKey generate_new_data_key(Yb::Session &session);
DEKPoolStatus get_dek_pool_status(Yb::Session &session);

class CardCrypter {
public:
    CardCrypter(Yb::Session &session);
    ~CardCrypter();

    std::string get_token(const CardData &card_data);
    CardData get_card(const std::string &token);
    void change_master_key();

private:
    Yb::Session &session;
    std::string master_key;

    std::string encrypt_pan(AESCrypter &crypter, const std::string &pan);
};

#endif
