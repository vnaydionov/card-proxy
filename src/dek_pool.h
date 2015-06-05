// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__DEK_POOL_H
#define CARD_PROXY__DEK_POOL_H

#include "domain/DataKey.h"

struct DEKPoolStatus;

class DEKPool {
public:
    static DEKPool* get_instance();
    static DEKPool* get_instance(Yb::Session &session);
    static bool delete_instance();

    const DEKPoolStatus get_status();
    const DEKPoolStatus check_pool();
    Domain::DataKey get_active_data_key();
    
    int get_max_active_dek_count();
    int get_auto_generation_threshold();
    int get_man_generation_threshold();
    int get_check_timeout();

    void set_max_active_dek_count(unsigned val);
    void set_auto_generation_threshold(unsigned val);
    void set_man_generation_threshold(unsigned val);
    void set_check_timeout(unsigned val);
    
private:
    static DEKPool * instance_;

    DEKPool(Yb::Session &session);
    DEKPool(const DEKPool &);
    DEKPool& operator=(DEKPool&);

    Domain::DataKey generate_new_data_key();

    Yb::Session &_session;
    int dek_use_count_;
    int max_active_dek_count_;
    int auto_generation_limit_;
    int man_generation_limit_;
    int check_timeout_;
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
