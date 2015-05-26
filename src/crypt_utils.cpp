#include "crypt_utils.h"
#include "helpers.h"

using namespace Domain;

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

Domain::DataKey generate_new_dek(Yb::Session &session) {
    Domain::DataKey data_key;
    AESCrypter aes_crypter;
    std::string master_key = assemble_master_key();
    std::string dek_value = generate_dek_value();
    aes_crypter.set_master_key(master_key);
    std::string crypted_dek = aes_crypter.encrypt(dek_value);
    std::string encoded_dek = encode_base64(crypted_dek);
    
    data_key.dek_crypted = encoded_dek;
    data_key.start_ts = Yb::now();
    data_key.finish_ts = Yb::dt_make(2020, 12, 31);
    data_key.counter = 0;
    data_key.save(session);
    session.commit();
}

Domain::DataKey get_active_dek() {
    std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
    DEKPoolStatus dek_status = get_dek_pool_status(*session);
    //lol
    while(dek_status.active_count < 50) { 
        generate_new_dek(*session);
        dek_status = get_dek_pool_status(*session);
    }

    //add here uberlogic
    DataKey dek = Yb::query<DataKey>(*session)
            .filter_by(DataKey::c.counter < 10)
            .order_by(DataKey::c.counter)
            .one();
    return dek;
}
