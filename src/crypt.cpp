#include "crypt.h"
#include "helpers.h"

using namespace Domain;

#define DEK_USE_COUNT 10
#define MAX_DEK_COUNT 500
#define AUTO_GEN_LIMIT 50
#define MAN_GEN_LIMIT 100

CardCrypter::CardCrypter(Yb::Session &session) : session(session) {
    _master_key = assemble_master_key();
}

CardCrypter::~CardCrypter() {
}

Card CardCrypter::get_token(const CardData &card_data) {
    AESCrypter master_crypter(_master_key);

    //check card in db
    Card card = _save_card(master_crypter, card_data);
    IncomingRequest incoming_request = _save_cvn(master_crypter,
        card, card_data._cvn);

    return card; 
}

Card CardCrypter::_save_card(AESCrypter &master_crypter, const CardData &card_data) {
    Card card;
    AESCrypter data_crypter;
    DEKPool dek_pool(session);
    DataKey data_key = dek_pool.get_active_data_key();
    std::string pure_dek = _get_decoded_dek(master_crypter, data_key.dek_crypted);
    data_crypter.set_key(pure_dek);
    std::string crypted_pan = _get_encoded_str(data_crypter, card_data._pan);

    card.card_token = _generate_card_token();
    card.ts = Yb::now();
    card.expire_dt = Yb::dt_make(2020, 12, 31); //TODO
    card.card_holder = card_data._chname;
    card.pan_masked = card_data._masked_pan;
    card.pan_crypted = crypted_pan;
    card.dek = DataKey::Holder(data_key);
    card.save(session);
    data_key.counter = data_key.counter + 1; session.commit();
    return card;
}

IncomingRequest CardCrypter::_save_cvn(AESCrypter &master_crypter,
    Card &card, const std::string &cvn) {
    IncomingRequest incoming_request;
    AESCrypter data_crypter;
    DEKPool dek_pool(session);
    DataKey data_key = dek_pool.get_active_data_key();
    std::string pure_dek = _get_decoded_dek(master_crypter, data_key.dek_crypted);
    data_crypter.set_key(pure_dek);
    std::string crypted_cvn = _get_encoded_str(data_crypter, cvn);

    incoming_request.ts = Yb::now();
    incoming_request.cvn_crypted = crypted_cvn;
    incoming_request.dek = DataKey::Holder(data_key);
    incoming_request.card = Card::Holder(card);
    incoming_request.save(session);
    data_key.counter = data_key.counter + 1;
    session.commit();
    return incoming_request;
}

CardData CardCrypter::get_card(const std::string &token) {
    AESCrypter master_crypter(_master_key); 
    DEKPool dek_pool(session);
    Card card = Yb::query<Card>(session)
            .filter_by(Card::c.card_token == token)
            .one();
    IncomingRequest incoming_request = *card.cvn;

    std::string pure_card_dek = _get_decoded_dek(master_crypter, card.dek->dek_crypted);
    std::string pure_cvn_dek = _get_decoded_dek(master_crypter, incoming_request.dek->dek_crypted);
    AESCrypter card_crypter(pure_card_dek), cvn_crypter(pure_cvn_dek);
    std::string pure_pan = _get_decoded_str(card_crypter, card.pan_crypted); 
    std::string pure_cvn = _get_decoded_str(cvn_crypter, incoming_request.cvn_crypted); 

    CardData card_data(card.card_holder.value(), pure_pan, "",
        Yb::to_string(card.expire_dt.value()), pure_cvn);
    return card_data;
}

std::string CardCrypter::_generate_card_token() {
    return generate_random_number(20);
}

std::string CardCrypter::_get_encoded_str(AESCrypter &crypter, const std::string &str) {
    std::string bindec_str, base64_str;
    try {
        bindec_str = BinDecConverter().encode(str);
        base64_str = encode_base64(crypter.encrypt(bindec_str));
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.to_string() << std::endl;
    }
    return base64_str;
}

std::string CardCrypter::_get_decoded_str(AESCrypter &crypter, const std::string &str) {
    std::string bindec_str, pure_str;
    try {
        bindec_str = crypter.decrypt(decode_base64(str));
        pure_str = BinDecConverter().decode(bindec_str);
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.to_string() << std::endl;
    }
    return pure_str;
}

std::string CardCrypter::_get_decoded_dek(AESCrypter &crypter, const std::string &dek) {
    std::string pure_dek;
    try {
        pure_dek = crypter.decrypt(decode_base64(dek));
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.to_string() << std::endl;
    }
    return pure_dek;
}

std::string assemble_master_key() {
    // make there UBER LOGIC FOR COMPOSE MASTER KEY
    return "12345678901234567890123456789012"; // 32 bytes
}

DEKPool::DEKPool(Yb::Session &session) 
    : _session(session)
    , _dek_use_count(DEK_USE_COUNT)
    , _max_active_dek_count(MAX_DEK_COUNT)
    , _auto_generation_limit(AUTO_GEN_LIMIT)
    , _man_generation_limit(MAN_GEN_LIMIT) {
}

DEKPool::~DEKPool() {
}

DataKey DEKPool::get_active_data_key() {
    DEKPoolStatus pool_status = _check_pool();

    auto active_deks = Yb::query<DataKey>(_session)
            .filter_by(DataKey::c.counter < static_cast<int>(_dek_use_count));
    int choice = rand() % pool_status.use_count;
    for(auto &dek : active_deks.all()) {
        choice -= (_dek_use_count - dek.counter);
        if(choice <= 0)
            return dek;
    }
    //errrrrr
}

DEKPoolStatus DEKPool::_check_pool() {
    DEKPoolStatus pool_status = get_status();
    while(pool_status.use_count < _auto_generation_limit) {
        _generate_new_data_key();
        pool_status = get_status();
    }
    return pool_status;
}

DEKPoolStatus DEKPool::get_status() {
    int count = 0;
    auto total_query = Yb::query<DataKey>(_session);
    auto active_query = total_query
            .filter_by(DataKey::c.counter < static_cast<int>(_dek_use_count));
    for(auto &date_key : active_query.all())
        count += _dek_use_count - date_key.counter;
    return DEKPoolStatus(total_query.count(), active_query.count(), count);
}

DataKey DEKPool::_generate_new_data_key() {
    DataKey data_key;
    std::string master_key = assemble_master_key();
    std::string dek_value = generate_dek_value(32);
    std::string encoded_dek;
    try {
        AESCrypter aes_crypter(master_key);
        encoded_dek = encode_base64(aes_crypter.encrypt(dek_value));
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.to_string() << std::endl;
    }
    data_key.dek_crypted = encoded_dek;
    data_key.start_ts = Yb::now();
    data_key.finish_ts = Yb::dt_make(2020, 12, 31);
    data_key.counter = 0;
    data_key.save(_session);
    _session.commit();
    return data_key;
}


CardData generate_random_card_data() {
    std::string chname = generate_random_string(10) + " " +
        generate_random_string(10);
    std::string pan = generate_random_number(16);
    std::string masked_pan = pan;
    std::string expdate = "2020-12-31T00:00:00";//Yb::dt_make(2020, 12, 31);
    std::string cvn = generate_random_number(3);
    return CardData(chname, pan, masked_pan, expdate, cvn);
}
