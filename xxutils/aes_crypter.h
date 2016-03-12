// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__AES_CRYPTER_H
#define CARD_PROXY__AES_CRYPTER_H

#include <string>
#include <stdexcept>
#include "utils.h"

#define AES_CRYPTER_BLOCK_SIZE 128
#define AES_CRYPTER_BLOCK_SIZE_BYTES (AES_CRYPTER_BLOCK_SIZE / 8)
#define AES_CRYPTER_KEY_SIZE 256
#define AES_CRYPTER_KEY_SIZE_BYTES (AES_CRYPTER_KEY_SIZE / 8)

class AESCrypter
{
public:
    AESCrypter(const std::string &key);

    std::string encrypt(const std::string &input_text);
    std::string decrypt(const std::string &input_cipher);

private:
    std::string encrypt_key_, decrypt_key_;
};

class AESBlockSizeException: public RunTimeError
{
public:
    AESBlockSizeException(int expected_size, const std::string &msg);
};

const std::string sha256_digest(const std::string &s);
std::string &xor_buffer(std::string &buf, const std::string &second);
const std::string hmac_sha256_digest(const std::string &hk, const std::string &s);

#endif // CARD_PROXY__AES_CRYPTER_H
// vim:ts=4:sts=4:sw=4:et:
