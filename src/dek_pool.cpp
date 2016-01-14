// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "aes_crypter.h"
#include "card_crypter.h"
#include "dek_pool.h"
#include "utils.h"

DEKPool::DEKPool(IConfig &config, Yb::ILogger &logger,
                 Yb::Session &session, const std::string &master_key)
    : config_(config)
    , logger_(logger.new_logger("dek_pool").release())
    , session_(session)
    , master_key_(master_key)
    , dek_use_count_(config.get_value_as_int("Dek/UseCount"))
    , min_active_dek_count_(config.get_value_as_int("Dek/MinLimit"))
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
        // the more used DEKs are found in the "upper" half of the list,
        // they get chosen twice as likely as DEKs from the "lower" half
        int half_size = active_deks_.size() / 2;
        bool upper_half = (rand() % 3) > 0;
        int choice = upper_half ? ((rand() % (active_deks_.size() - half_size))
                                   + half_size)
                                : (rand() % half_size);
        Domain::DataKey dek = Yb::lock_and_refresh(session_, active_deks_[choice]);
        if (dek.counter < dek_use_count_ && dek.finish_ts > Yb::now()) {
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
        .filter_by(Domain::DataKey::c.counter < dek_use_count_)
        .filter_by(Domain::DataKey::c.finish_ts > Yb::now())
        .order_by(Domain::DataKey::c.counter)
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
        int new_data_keys_needed = (min_active_dek_count_ - unused_count
                + dek_use_count_ - 1) / dek_use_count_;
        for (int i = 0; i < new_data_keys_needed; ++i)
            generate_new_data_key();  // don't flush
        session_.flush();
        fill_active_deks();
    }
}

Domain::DataKey DEKPool::generate_new_data_key() {
    Domain::DataKey data_key;
    std::string encoded_dek;
    std::string dek_value = generate_random_bytes(32);
    AESCrypter aes_crypter(master_key_);
    encoded_dek = encode_base64(aes_crypter.encrypt(dek_value));
    data_key.dek_crypted = encoded_dek;
    data_key.start_ts = Yb::now();
    data_key.finish_ts = Yb::dt_make(2020, 12, 31);
    data_key.counter = 0;
    data_key.save(session_);
    return data_key;
}

// vim:ts=4:sts=4:sw=4:et:
