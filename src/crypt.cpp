#include "crypt.h"
#include "helpers.h"

using namespace Domain;

CardCrypter::CardCrypter(Yb::Session &session) : session(session) {
    master_key = assemble_master_key();
}

CardCrypter::~CardCrypter() {
}

std::string CardCrypter::get_token(const CardData &card_data) {
    Card card;
    AESCrypter master_crypter(master_key); 
    DataKey data_key = get_active_data_key(session);
    std::string aes_data_key = decode_base64(data_key.dek_crypted);
    std::string pure_data_key = master_crypter.decrypt(aes_data_key);
    AESCrypter data_key_crypter(pure_data_key);
    
    card.card_token = 999999;
    card.ts = Yb::now();
    card.expire_dt = Yb::dt_make(2020, 12, 31); //TODO
    card.card_holder = card_data._chname;
    card.pan_masked = card_data._masked_pan;
    card.pan_crypted = encrypt_pan(data_key_crypter, card_data._pan);
    //card.dek_id = data_key.id;
    card.save(session);
    session.commit();

    return std::to_string(card.card_token.value()); 
}

CardData CardCrypter::get_card(const std::string &token) {
    CardData card;
    return card;
}

std::string CardCrypter::encrypt_pan(AESCrypter &crypter, const std::string &pan) {
   std::string bindec_pan = BinDecConverter().encode(pan);
   std::string aes_pan = crypter.encrypt(bindec_pan);
   std::string base64_pan = encode_base64(aes_pan);
   return base64_pan;
}

std::string assemble_master_key() {
    // make there UBER LOGIC FOR COMPOSE MASTER KEY
    return "12345678901234567890123456789012"; // 32 bytes
}

DEKPoolStatus get_dek_pool_status(Yb::Session &session) {
    DEKPoolStatus dek_status;
    int active_use_count = 0;
    auto total_query = Yb::query<DataKey>(session);
    auto active_query = total_query.filter_by(DataKey::c.counter < 10);
    for(auto &date_key : active_query.all())
        active_use_count += 10 - date_key.counter;
    dek_status.total_count= total_query.count();
    dek_status.active_count = active_query.count();
    dek_status.use_count = active_use_count;
    return dek_status;
}

Domain::DataKey generate_new_data_key(Yb::Session &session) {
    Domain::DataKey data_key;
    AESCrypter aes_crypter;
    std::string master_key = assemble_master_key();
    std::string dek_value = generate_dek_value(32);
    try {
        aes_crypter.set_master_key(master_key);
        std::string crypted_dek = aes_crypter.encrypt(dek_value);
        std::string encoded_dek = encode_base64(crypted_dek);
        data_key.dek_crypted = encoded_dek;
        data_key.start_ts = Yb::now();
        data_key.finish_ts = Yb::dt_make(2020, 12, 31);
        data_key.counter = 0;
        data_key.save(session);
        session.commit();
    } catch(AESBlockSizeException &exc) {
        std::cout << "Invalid block size: " << exc.get_string().length()
            << ", expected %" << exc.get_block_size() << " ["
            << string_to_hexstring(exc.get_string()) << "]" << std::endl;
    }
    return data_key;
}

Domain::DataKey get_active_data_key(Yb::Session &session) {
    DEKPoolStatus dek_status = get_dek_pool_status(session);
    //lol
    while(dek_status.active_count < 50) { 
        generate_new_data_key(session);
        dek_status = get_dek_pool_status(session);
    }

    //add here uberlogic
    DataKey dek = Yb::query<DataKey>(session)
            .filter_by(DataKey::c.counter < 10)
            .order_by(DataKey::c.counter)
            .range(0, 1).one();
    return dek;
}
