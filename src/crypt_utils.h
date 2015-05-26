#ifndef CRYPT_UTILS_H
#define CRYPT_UTILS_H
#include <string>

#include <orm/data_object.h>
#include <orm/domain_object.h>

#include "utils.h"
#include "aes_crypter.h"
#include "domain/DataKey.h"


struct DEKPoolStatus {
    long total_count;
    long active_count;
    long use_count;
};

std::string assemble_master_key();

Domain::DataKey get_active_dek();

Domain::DataKey generate_new_dek(Yb::Session &session);

DEKPoolStatus get_dek_pool_status(Yb::Session &session);
#endif
