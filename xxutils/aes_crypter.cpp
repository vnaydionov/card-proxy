// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <iostream>
#include <util/string_utils.h>

#include "aes_crypter.h"
#include "utils.h"

#define TO_AES_KEY(s) reinterpret_cast<AES_KEY *>(&((s)[0]))
#define TO_CONST_UCHAR(s) reinterpret_cast<const unsigned char *>((s).data())
#define TO_UCHAR(s) reinterpret_cast<unsigned char *>(&((s)[0]))

AESCrypter::AESCrypter(const std::string &key)
    : encrypt_key_(sizeof(AES_KEY), 0)
    , decrypt_key_(sizeof(AES_KEY), 0)
{
    if (key.size() != AES_CRYPTER_KEY_SIZE_BYTES)
        throw AESBlockSizeException(AES_CRYPTER_KEY_SIZE_BYTES,
                "expected AES key block size");
    AES_set_encrypt_key(TO_CONST_UCHAR(key), AES_CRYPTER_KEY_SIZE,
            TO_AES_KEY(encrypt_key_));
    AES_set_decrypt_key(TO_CONST_UCHAR(key), AES_CRYPTER_KEY_SIZE,
            TO_AES_KEY(decrypt_key_));
}

std::string AESCrypter::encrypt(const std::string &input_text)
{
    if (!input_text.size()
            || input_text.size() % AES_CRYPTER_BLOCK_SIZE_BYTES != 0)
    {
        throw AESBlockSizeException(AES_CRYPTER_BLOCK_SIZE_BYTES,
                "input text size must be a multiple of AES block size");
    }
    std::string result(input_text.size(), 0);
    for (size_t offs = 0; offs < input_text.size();
            offs += AES_CRYPTER_BLOCK_SIZE_BYTES)
    {
        AES_encrypt(TO_CONST_UCHAR(input_text) + offs,
                TO_UCHAR(result) + offs, TO_AES_KEY(encrypt_key_));
    }
    return result;
}

std::string AESCrypter::decrypt(const std::string &input_cipher)
{
    if (!input_cipher.size()
            || input_cipher.size() % AES_CRYPTER_BLOCK_SIZE_BYTES != 0)
    {
        throw AESBlockSizeException(AES_CRYPTER_BLOCK_SIZE_BYTES,
                "input cipher size must be a multiple of AES block size");
    }
    std::string result(input_cipher.size(), 0);
    for (size_t offs = 0; offs < input_cipher.size();
            offs += AES_CRYPTER_BLOCK_SIZE_BYTES)
    {
        AES_decrypt(TO_CONST_UCHAR(input_cipher) + offs,
                TO_UCHAR(result) + offs, TO_AES_KEY(decrypt_key_));
    }
    return result;
}

AESBlockSizeException::AESBlockSizeException(int expected_size, const std::string &msg)
    : RunTimeError(msg + ": " + Yb::to_string(expected_size))
{}

const std::string sha256_digest(const std::string &s)
{
    std::string out;
    out.resize(SHA256_DIGEST_LENGTH);
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, s.data(), s.size());
    SHA256_Final((unsigned char *)&out[0], &ctx);
    return out;
}

std::string &xor_buffer(std::string &buf, const std::string &second)
{
    YB_ASSERT(buf.size() == second.size());
    auto i = buf.begin(), iend = buf.end();
    auto j = second.begin();
    for (; i != iend; ++i, ++j)
        *i ^= *j;
    return buf;
}

const std::string hmac_sha256_digest(const std::string &hk, const std::string &s)
{
    const size_t block_size = 64;
    std::string key = hk;
    if (key.size() > block_size)
        key = sha256_digest(key);
    if (key.size() < block_size)
        key += std::string(block_size - key.size(), 0);
    std::string o_key_pad(block_size, 0x5c);
    xor_buffer(o_key_pad, key);
    std::string i_key_pad(block_size, 0x36);
    xor_buffer(i_key_pad, key);
    return sha256_digest(o_key_pad + sha256_digest(i_key_pad + s));
}

// vim:ts=4:sts=4:sw=4:et:
