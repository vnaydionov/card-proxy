// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "aes_crypter.h"
#include "card_crypter.h"
#include "dek_pool.h"
#include "utils.h"


//DEKPool *DEKPool::instance_ = 0;

DEKPool::DEKPool(IConfig &config, Yb::Session &session)
    : config_(config)
    , session_(session)
{
    config.reload();
    dek_use_count_ = config.get_value_as_int("dek/use_count");
    max_active_dek_count_ = config.get_value_as_int("dek/max_limit");
    min_active_dek_count_ = config.get_value_as_int("dek/min_limit");
}

//DEKPool* DEKPool::get_instance() {
//    if (!instance_)
//        throw std::runtime_error("Instance require Yb::Session object");
//    return instance_;
//}
//
//DEKPool* DEKPool::get_instance(Yb::Session &session) {
//    if (!instance_)
//        instance_= new DEKPool(session);
//    return instance_;
//}

Domain::DataKey DEKPool::get_active_data_key() {
    DEKPoolStatus pool_status = check_pool();
    if (!pool_status.active_count)
        throw std::runtime_error("DEK pool is exhausted");
    auto active_deks = Yb::query<Domain::DataKey>(session_)
            .filter_by(Domain::DataKey::c.counter < dek_use_count_);
    int choice = rand() % pool_status.active_count;
    for(auto &dek : active_deks.all()) {
        //choice -= (dek_use_count_ - dek.counter);
        if (--choice <= 0)
            return dek;
    }
    throw std::runtime_error("Cannot obtain active DEK");
}

const DEKPoolStatus DEKPool::check_pool() {
    DEKPoolStatus pool_status = get_status();
    while(pool_status.use_count < min_active_dek_count_) {
        generate_new_data_key();
        pool_status = get_status();
    }
    return pool_status;
}

const DEKPoolStatus DEKPool::get_status() {
    int count = 0, use_count = static_cast<int>(dek_use_count_);
    auto total_query = Yb::query<Domain::DataKey>(session_);
    auto active_query = total_query
            .filter_by(Domain::DataKey::c.counter < use_count);
    for(auto &data_key : active_query.all())
        count += dek_use_count_ - data_key.counter;
    return DEKPoolStatus(total_query.count(), active_query.count(), count);
}

Domain::DataKey DEKPool::generate_new_data_key() {
    Domain::DataKey data_key;
    std::string master_key = CardCrypter::assemble_master_key(config_, session_);
    std::string dek_value = generate_random_bytes(32);
    std::string encoded_dek;
    try {
        AESCrypter aes_crypter(master_key);
        encoded_dek = encode_base64(aes_crypter.encrypt(dek_value));
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.what() << std::endl;
    }
    data_key.dek_crypted = encoded_dek;
    data_key.start_ts = Yb::now();
    data_key.finish_ts = Yb::dt_make(2020, 12, 31);
    data_key.counter = 0;
    data_key.save(session_);
    session_.flush();
    return data_key;
}

// vim:ts=4:sts=4:sw=4:et:
