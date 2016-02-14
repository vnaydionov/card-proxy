// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "aes_crypter.h"
#include "dek_pool.h"
#include "utils.h"

DEKPool::DEKPool(IConfig &config, Yb::ILogger &logger,
                 Yb::Session &session, const std::string &master_key,
                 int kek_version)
    : config_(config)
    , logger_(logger.new_logger("dek_pool").release())
    , session_(session)
    , master_key_(master_key)
    , kek_version_(kek_version)
    , dek_use_count_(config.get_value_as_int("Dek/UseCount"))
    , min_active_dek_count_(config.get_value_as_int("Dek/MinActiveLimit"))
    , dek_usage_period_(config.get_value_as_int("Dek/UsagePeriod"))
{}

const DEKPoolStatus DEKPool::get_status() {
    auto active_deks_rs = query_active_deks();
    Domain::DataKey::List active_deks;
    std::copy(active_deks_rs.begin(), active_deks_rs.end(),
              std::back_inserter(active_deks));
    int unused_count = get_unused_count(active_deks);
    auto total_query = Yb::query<Domain::DataKey>(session_);
    return DEKPoolStatus(total_query.count(), active_deks.size(), unused_count);
}

Domain::DataKey DEKPool::get_active_data_key() {
    while (true) {
        generate_enough_deks();  // generate some if necessary
        int choice = rand() % active_deks_.size();
        Domain::DataKey dek = Yb::lock_and_refresh(session_, active_deks_[choice]);
        if (dek.counter < dek.max_counter && dek.finish_ts > Yb::now()) {
            session_.debug("selected DEK: " + Yb::to_string(dek.id.value()));
            return dek;
        }
        else {
            session_.debug("candidate DEK " + Yb::to_string(dek.id.value())
                    + " is invalid, try again");  // race condition detected
        }
    }
    throw RunTimeError("Cannot obtain active DEK");  // to avoid compiler warning
}

Domain::DataKey::ResultSet DEKPool::query_active_deks() {
    return Yb::query<Domain::DataKey>(session_)
        .filter_by(Domain::DataKey::c.counter < Domain::DataKey::c.max_counter)
        .filter_by(Domain::DataKey::c.finish_ts > Yb::now())
        .all();
}

void DEKPool::fill_active_deks() {
    active_deks_.clear();
    auto active_deks_rs = query_active_deks();
    std::copy(active_deks_rs.begin(), active_deks_rs.end(),
              std::back_inserter(active_deks_));
}

void DEKPool::generate_enough_deks() {
    if (!active_deks_.size())
        fill_active_deks();
    while (true) {
        int unused_count = get_unused_count(active_deks_);
        if (unused_count >= min_active_dek_count_)
            break;
        generate_new_data_key();
        generate_new_data_key();
        session_.flush();
        fill_active_deks();
        break;
    }
}

Domain::DataKey DEKPool::generate_new_data_key(bool is_hmac) {
    Domain::DataKey data_key;
    std::string dek_value = generate_random_bytes(32);
    AESCrypter aes_crypter(master_key_);
    std::string encoded_dek = encode_base64(aes_crypter.encrypt(dek_value));
    data_key.start_ts = Yb::now();
    if (is_hmac) {
        data_key.finish_ts = Yb::dt_add_seconds(data_key.start_ts,
                366 * 24 * 3600);
        data_key.max_counter = 0;
    }
    else {
        data_key.finish_ts = Yb::dt_add_seconds(data_key.start_ts,
                dek_usage_period_ * 24 * 3600);
        data_key.max_counter = dek_use_count();
    }
    data_key.dek_crypted = encoded_dek;
    data_key.kek_version = kek_version_;
    data_key.counter = 0;
    data_key.save(session_);
    return data_key;
}

// vim:ts=4:sts=4:sw=4:et:
