// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "dek_pool.h"
#include "utils.h"
#include "aes_crypter.h"
#include "crypt.h"

#define DEK_USE_COUNT 10
#define MAX_DEK_COUNT 500
#define AUTO_GEN_LIMIT 50
#define MAN_GEN_LIMIT 100


DEKPool::DEKPool(Yb::Session &session)
    : _session(session)
    , _dek_use_count(DEK_USE_COUNT)
    , _max_active_dek_count(MAX_DEK_COUNT)
    , _auto_generation_limit(AUTO_GEN_LIMIT)
    , _man_generation_limit(MAN_GEN_LIMIT) {
}

DEKPool::~DEKPool() {
}

Domain::DataKey DEKPool::get_active_data_key() {
    DEKPoolStatus pool_status = _check_pool();

    auto active_deks = Yb::query<Domain::DataKey>(_session)
            .filter_by(Domain::DataKey::c.counter < static_cast<int>(_dek_use_count));
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
    auto total_query = Yb::query<Domain::DataKey>(_session);
    auto active_query = total_query
            .filter_by(Domain::DataKey::c.counter < static_cast<int>(_dek_use_count));
    for(auto &data_key : active_query.all())
        count += _dek_use_count - data_key.counter;
    return DEKPoolStatus(total_query.count(), active_query.count(), count);
}

Domain::DataKey DEKPool::_generate_new_data_key() {
    Domain::DataKey data_key;
    std::string master_key = CardCrypter::assemble_master_key(_session);
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
    data_key.save(_session);
    _session.commit();
    return data_key;
}

// vim:ts=4:sts=4:sw=4:et:
