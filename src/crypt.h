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
    DEKPoolStatus() : total_count(0), active_count(0), use_count(0) {}
    DEKPoolStatus(const int &total, const int &active, const int &use) 
    : total_count(total), active_count(active), use_count(use) {}
};

std::string assemble_master_key();

std::string generate_token();

class CardCrypter {
public:
    CardCrypter(Yb::Session &session);
    ~CardCrypter();

    std::string get_token(const CardData &card_data);
    CardData get_card(const std::string &token);
    void change_master_key();

private:
    Yb::Session &session;
    std::string _master_key;

    std::string _get_encoded_pan(AESCrypter &crypter, const std::string &pan);
    std::string _get_decoded_pan(AESCrypter &crypter, const std::string &pan);
    std::string _get_decoded_dek(AESCrypter &crypter, const std::string &dek);
};

class DEKPool {
//singleton?
public:
    DEKPool(Yb::Session &session);
    ~DEKPool();

    DEKPoolStatus get_status();
    Domain::DataKey get_active_data_key();
    
    unsigned get_max_active_dek_count();
    unsigned get_auto_generation_threshold();
    unsigned get_man_generation_threshold();
    unsigned get_check_timeout();

    void set_max_active_dek_count(unsigned val);
    void set_auto_generation_threshold(unsigned val);
    void set_man_generation_threshold(unsigned val);
    void set_check_timeout(unsigned val);
    
private:
    Domain::DataKey _generate_new_data_key();
    DEKPoolStatus _check_pool();

    Yb::Session &_session;
    unsigned _dek_use_count;
    unsigned _max_active_dek_count;
    unsigned _auto_generation_limit;
    unsigned _man_generation_limit;
    unsigned _check_timeout;
};
#endif
