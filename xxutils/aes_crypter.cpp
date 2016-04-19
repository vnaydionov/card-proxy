// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <iostream>
#include <util/string_utils.h>

#include "aes_crypter.h"
#include "utils.h"

#define TO_CONST_AES_KEY(s) reinterpret_cast<const AES_KEY *>((s).c_str())
#define TO_CONST_UCHAR(s) reinterpret_cast<const unsigned char *>((s).c_str())

AESCrypter::AESCrypter(const std::string &key, int mode)
    : mode_(mode)
{
    if (key.size() != AES_CRYPTER_KEY_SIZE_BYTES)
        throw AESBlockSizeException(AES_CRYPTER_KEY_SIZE_BYTES,
                "expected AES key block size");
    AES_KEY keybuf;
    AES_set_encrypt_key(TO_CONST_UCHAR(key), AES_CRYPTER_KEY_SIZE,
                        &keybuf);
    encrypt_key_ = std::string((const char *)&keybuf, sizeof(AES_KEY));
    AES_set_decrypt_key(TO_CONST_UCHAR(key), AES_CRYPTER_KEY_SIZE,
                        &keybuf);
    decrypt_key_ = std::string((const char *)&keybuf, sizeof(AES_KEY));
    memset(&keybuf, 0, sizeof(AES_KEY));  // TODO: secure_erase
}

std::string AESCrypter::encrypt(const std::string &input_text)
{
    if (!input_text.size()
            || input_text.size() % AES_CRYPTER_BLOCK_SIZE_BYTES != 0)
    {
        throw AESBlockSizeException(AES_CRYPTER_BLOCK_SIZE_BYTES,
                "input text size must be a multiple of AES block size");
    }
    unsigned char *result = new unsigned char[input_text.size()];
    try {
        memset(result, 0, input_text.size());
        unsigned char iv[AES_CRYPTER_BLOCK_SIZE_BYTES];
        memset(iv, 0, AES_CRYPTER_BLOCK_SIZE_BYTES);
        for (size_t offs = 0; offs < input_text.size();
                offs += AES_CRYPTER_BLOCK_SIZE_BYTES)
        {
            if (mode_ == AES_CRYPTER_CBC)
            {
                memxor(iv, TO_CONST_UCHAR(input_text) + offs, AES_CRYPTER_BLOCK_SIZE_BYTES);
            }
            else
            {
                memcpy(iv, TO_CONST_UCHAR(input_text) + offs, AES_CRYPTER_BLOCK_SIZE_BYTES);
            }
            AES_encrypt(iv,
                        result + offs, TO_CONST_AES_KEY(encrypt_key_));
            if (mode_ == AES_CRYPTER_CBC)
            {
                memcpy(iv, result + offs, AES_CRYPTER_BLOCK_SIZE_BYTES);
            }
        }
        std::string r((const char *)result, input_text.size());
        memset(result, 0, input_text.size());  // TODO: secure_erase
        delete [] result;
        return r;
    }
    catch (...) {
        memset(result, 0, input_text.size());  // TODO: secure_erase
        delete [] result;
        throw;
    }
}

std::string AESCrypter::decrypt(const std::string &input_cipher)
{
    if (!input_cipher.size()
            || input_cipher.size() % AES_CRYPTER_BLOCK_SIZE_BYTES != 0)
    {
        throw AESBlockSizeException(AES_CRYPTER_BLOCK_SIZE_BYTES,
                "input cipher size must be a multiple of AES block size");
    }
    unsigned char *result = new unsigned char[input_cipher.size()];
    try {
        memset(result, 0, input_cipher.size());
        unsigned char iv[AES_CRYPTER_BLOCK_SIZE_BYTES];
        memset(iv, 0, AES_CRYPTER_BLOCK_SIZE_BYTES);
        unsigned char iv_next[AES_CRYPTER_BLOCK_SIZE_BYTES];
        memset(iv_next, 0, AES_CRYPTER_BLOCK_SIZE_BYTES);
        for (size_t offs = 0; offs < input_cipher.size();
                offs += AES_CRYPTER_BLOCK_SIZE_BYTES)
        {
            memcpy(iv_next, TO_CONST_UCHAR(input_cipher) + offs,
                    AES_CRYPTER_BLOCK_SIZE_BYTES);
            AES_decrypt(iv_next,
                        result + offs, TO_CONST_AES_KEY(decrypt_key_));
            if (mode_ == AES_CRYPTER_CBC)
            {
                memxor(result + offs, iv, AES_CRYPTER_BLOCK_SIZE_BYTES);
                memcpy(iv, iv_next, AES_CRYPTER_BLOCK_SIZE_BYTES);
            }
        }
        std::string r((const char *)result, input_cipher.size());
        memset(result, 0, input_cipher.size());  // TODO: secure_erase
        delete [] result;
        return r;
    }
    catch (...) {
        memset(result, 0, input_cipher.size());  // TODO: secure_erase
        delete [] result;
        throw;
    }
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

std::string bcd_decode(const std::string &bcd_input)
{
    if (!bcd_input.size())
        return std::string(); // TODO analyze this case
    if (bcd_input.size() != AES_CRYPTER_BLOCK_SIZE_BYTES)
        throw RunTimeError("BCD decode data of wrong size");
    std::string x = string_to_hexstring(bcd_input,
                                        HEX_LOWERCASE|HEX_NOSPACES);
    size_t i = 0;
    for (; i < x.size(); ++i)
        if (!(x[i] >= '0' && x[i] <= '9'))
            break;
    return x.substr(0, i);
}

std::string bcd_encode(const std::string &ascii_input)
{
    if (!ascii_input.size())
        return std::string();
    if (ascii_input.size() > AES_CRYPTER_BLOCK_SIZE_BYTES * 2)
        throw RunTimeError("BCD encode data of wrong size");
    std::string x = ascii_input + "f";  // TODO may be use a..f
    int padding_length = AES_CRYPTER_BLOCK_SIZE_BYTES * 2 - x.size();
    if (padding_length > 0)
        x += generate_random_hex_string(padding_length);
    return string_from_hexstring(
            x.substr(0, AES_CRYPTER_BLOCK_SIZE_BYTES * 2), HEX_NOSPACES);
}

std::string pkcs7_decode(const std::string &blocks_input)
{
    if (blocks_input.size() % AES_CRYPTER_BLOCK_SIZE_BYTES)
        throw RunTimeError("PKCS7 decode data of wrong size");
    char d = blocks_input[blocks_input.size() - 1];
    if (!(d >= 1 && d <= AES_CRYPTER_BLOCK_SIZE_BYTES))
        throw RunTimeError("PKCS7 padding not found");
    for (int i = blocks_input.size() - 2; i >= (int)blocks_input.size() - d; --i)
    {
        if (blocks_input[i] != d)
            throw RunTimeError("PKCS7 padding not found");
    }
    return blocks_input.substr(0, blocks_input.size() - d);
}

std::string pkcs7_encode(const std::string &plain_input)
{
    size_t padding_length = AES_CRYPTER_BLOCK_SIZE_BYTES -
        plain_input.size() % AES_CRYPTER_BLOCK_SIZE_BYTES;
    return plain_input + std::string(padding_length, (char)padding_length);
}

void *memxor(void *dst, const void *src, size_t len)
{
    void *dst0 = dst;
    unsigned char *cdst = (unsigned char *)dst,
        *cend = cdst + len;
    const unsigned char *csrc = (const unsigned char *)src;
    for (; cdst != cend; ++csrc, ++cdst)
        *cdst ^= *csrc;
    return dst0;
}

// vim:ts=4:sts=4:sw=4:et:
