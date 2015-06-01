// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <stdexcept>

#include "utils.h"

char int_to_hexchar(int digit, int mode)
{
    if (digit >= 0 && digit <= 9)
        return '0' + digit;
    else if (digit >= 10 && digit <= 15)
        return (!(mode & HEX_LOWERCASE)? 'A': 'a') + digit - 10;
    throw std::runtime_error("Invalid digit");
}

int hexchar_to_int(char symbol)
{
    if (symbol >= '0' && symbol <= '9')
        return symbol - '0';
    else if (symbol >= 'A' && symbol <= 'F')
        return symbol - 'A' + 10;
    else if (symbol >= 'a' && symbol <= 'f')
        return symbol - 'a' + 10;
    throw std::runtime_error("Invalid HEX digit character: " +
            std::to_string((int)(unsigned char)symbol));
}
 
std::string string_to_hexstring(const std::string &input, int mode)
{
    if (!input.size())
        return std::string();
    size_t in_size = input.size();
    bool insert_spaces = !(mode & HEX_NOSPACES);
    size_t result_len = in_size * 2;
    if (insert_spaces)
        result_len = in_size * 3 - 1;
    const char *str = input.data();
    std::string result(result_len, ' ');
    for (size_t i = 0; i < result_len; ++str) {
        result[i++] = int_to_hexchar((*str >> 4) & 0xF, mode);
        result[i++] = int_to_hexchar(*str & 0xF, mode);
        if (insert_spaces)
            ++i;
    }
    return result;
}

std::string string_from_hexstring(const std::string &hex_input, int mode)
{
    if (!hex_input.size())
        return std::string();
    size_t in_size = hex_input.size();
    bool insert_spaces = !(mode & HEX_NOSPACES);
    if (insert_spaces) {
        if (hex_input.size() % 3 != 2)
            throw std::runtime_error("HEX data of wrong size");
    }
    else {
        if (hex_input.size() % 2)
            throw std::runtime_error("HEX data of wrong size");
    }
    size_t result_len = in_size / 2;
    if (insert_spaces)
        result_len = (in_size + 1) / 3;
    const char *str = hex_input.data();
    std::string result(result_len, 0);
    for (size_t i = 0; i < result_len; ++i) {
        result[i] |= hexchar_to_int(*str++) << 4;
        result[i] |= hexchar_to_int(*str++);
        if (insert_spaces) {
            if (*str != ' ' && *str)
                throw std::runtime_error("HEX separator space is expected");
            ++str;
        }
    }
    return result;
}

std::string encode_base64(const std::string &message)
{
    if (!message.size())
        return std::string();
    int encoded_size = ((message.size() + 2) / 3) * 4;
    std::string result(encoded_size, 0);
    FILE *stream = fmemopen(&result[0], encoded_size + 1, "w");
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, message.data(), message.size());
    BIO_flush(bio);
    BIO_free_all(bio);
    fclose(stream);
    return result;
}

static bool valid_b64_char(char c)
{
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '+' || c == '/' || c == '=');
}

static int b64_decoded_size(const std::string &b64input)
{
    int length = b64input.size();
    int padding = 0;
    if (length > 1) {
        // Check for trailing '=''s as padding
        if (b64input[length - 1] == '=' && b64input[length - 2] == '=')
            padding = 2;
        else if (b64input[length - 1] == '=')
            padding = 1;
    }
    return (length / 4) * 3 - padding;
}

static void check_base64(const std::string &b64message)
{
    if (b64message.size() % 4)
        throw std::runtime_error("BASE64 data of wrong size");
    for (size_t i = 0; i < b64message.size(); ++i) {
        if (!valid_b64_char(b64message[i]))
            throw std::runtime_error("Invalid BASE64 character");
        if (b64message[i] == '=') {
            if (i < b64message.size() - 2 ||
                (i == b64message.size() - 2 && b64message[i + 1] != '='))
            {
                throw std::runtime_error("BASE64: Misplaced trailing =");
            }
        }
    }
}

std::string decode_base64(const std::string &b64message)
{
    if (!b64message.size())
        return std::string();
    check_base64(b64message);
    int decoded_size = b64_decoded_size(b64message);
    std::string result(decoded_size, 0);
    FILE *stream = fmemopen(const_cast<char *>(b64message.data()),
                            b64message.size(), "r");
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    char *buffer = &result[0];
    decoded_size = BIO_read(bio, buffer, b64message.size());
    buffer[decoded_size] = '\0';
    BIO_free_all(bio);
    fclose(stream);
    return result; 
}

std::string bcd_decode(const std::string &bcd_input)
{
    if (!bcd_input.size())
        return std::string();
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
    std::string x = ascii_input + "f";
    while (x.size() < 32)
        x = x + ascii_input + "f";
    return string_from_hexstring(x.substr(0, 32), HEX_NOSPACES);
}

std::string generate_random_bytes(size_t length)
{
    std::string result(length, 0);
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1)
        throw std::runtime_error("Can't open /dev/urandom");
    int n = read(fd, &result[0], result.size());
    close(fd);
    if (static_cast<int>(result.size()) != n)
        throw std::runtime_error("Can't read from /dev/urandom");
    return result;
}

std::string generate_random_number(size_t length)
{
    std::string result(length, 0);
    for (size_t i = 0; i < length; ++i) 
        result[i] = rand() % 10 + '0';
    return result;
}

std::string generate_random_string(size_t length)
{
    std::string result(length, 0);
    for (size_t i = 0; i < length; ++i) {
        int x = rand() % (26*2 + 10);
        if (x < 26)
            result[i] = 'A' + x;
        else if (x < 26*2)
            result[i] = 'a' + (x - 26);
        else
            result[i] = '0' + (x - 26*2);
    }
    return result;
}

std::string mask_pan(const std::string &pan)
{
    if (pan.size() < 13 || pan.size() > 20)
        throw std::runtime_error("Strange PAN length: " + std::to_string(pan.size()));
    return pan.substr(0, 6) + "**" + pan.substr(pan.size() - 4);
}

// vim:ts=4:sts=4:sw=4:et:
