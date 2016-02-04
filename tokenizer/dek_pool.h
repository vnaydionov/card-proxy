// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__DEK_POOL_H
#define CARD_PROXY__DEK_POOL_H

#include "util/nlogger.h"
#include "conf_reader.h"
#include "domain/DataKey.h"

struct DEKPoolStatus;

class DEKPool {
public:
    DEKPool(IConfig &config, Yb::ILogger &logger,
            Yb::Session &session, const std::string &master_key,
            int kek_version);

    int dek_use_count() const { return dek_use_count_; }
    int min_active_dek_count() const { return min_active_dek_count_; }

    const DEKPoolStatus get_status();
    Domain::DataKey generate_new_data_key();
    Domain::DataKey get_active_data_key();

private:
    // non-copyable object
    DEKPool(const DEKPool &);  
    DEKPool &operator=(const DEKPool &);

    template <class DataKeyContainer>
    int get_unused_count(DataKeyContainer &active_deks)
    {
        int unused_count = 0;
        auto i = active_deks.begin(), iend = active_deks.end();
        for (; i != iend; ++i)
            unused_count += dek_use_count_ - i->counter;
        return unused_count;
    }

    Domain::DataKey::ResultSet query_active_deks();
    void generate_enough_deks();
    void fill_active_deks();

    IConfig &config_;
    Yb::ILogger::Ptr logger_;
    Yb::Session &session_;
    std::string master_key_;
    int dek_use_count_;
    int min_active_dek_count_;
    int kek_version_;

    Domain::DataKey::List active_deks_;
};

struct DEKPoolStatus {
    long total_count;
    long active_count;
    long use_count;

    DEKPoolStatus() 
        : total_count(0)
        , active_count(0)
        , use_count(0) {
    }

    DEKPoolStatus(const int &total, const int &active, const int &use) 
        : total_count(total)
        , active_count(active)
        , use_count(use) {
    }
};

#endif // CARD_PROXY__DEK_POOL_H
// vim:ts=4:sts=4:sw=4:et:
