// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__DEK_POOL_H
#define CARD_PROXY__DEK_POOL_H

#include "domain/DataKey.h"

struct DEKPoolStatus
{
    long total_count;
    long active_count;
    long use_count;
    DEKPoolStatus() : total_count(0), active_count(0), use_count(0) {}
    DEKPoolStatus(int total, int active, int use) 
    : total_count(total), active_count(active), use_count(use) {}
};

class DEKPool
{
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

#endif // CARD_PROXY__DEK_POOL_H
// vim:ts=4:sts=4:sw=4:et:
