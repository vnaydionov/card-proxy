// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <fstream>
#include <iterator>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <util/string_utils.h>
#include "stack_trace.h"
#include "utils.h"

RunTimeError::RunTimeError(const std::string &msg)
    : runtime_error(msg + print_stacktrace())
{}

char int_to_hexchar(int digit, int mode)
{
    if (digit >= 0 && digit <= 9)
        return '0' + digit;
    else if (digit >= 10 && digit <= 15)
        return (!(mode & HEX_LOWERCASE)? 'A': 'a') + digit - 10;
    throw RunTimeError("Invalid digit");
}

int hexchar_to_int(char symbol)
{
    if (symbol >= '0' && symbol <= '9')
        return symbol - '0';
    else if (symbol >= 'A' && symbol <= 'F')
        return symbol - 'A' + 10;
    else if (symbol >= 'a' && symbol <= 'f')
        return symbol - 'a' + 10;
    throw RunTimeError("Invalid HEX digit character: " +
            Yb::to_string((int)(unsigned char)symbol));
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
            throw RunTimeError("HEX data of wrong size");
    }
    else {
        if (hex_input.size() % 2)
            throw RunTimeError("HEX data of wrong size");
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
                throw RunTimeError("HEX separator space is expected");
            ++str;
        }
    }
    return result;
}

std::string encode_base64(const std::string &message)
{
    if (!message.size())
        return std::string();
    size_t encoded_size = ((message.size() + 2) / 3) * 4;
    std::string result(encoded_size, 0);
    FILE *stream = fmemopen(&result[0], encoded_size + 1, "w");
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, message.data(), message.size());
    (void)BIO_flush(bio);
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
    size_t length = b64input.size();
    size_t padding = 0;
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
        throw RunTimeError("BASE64 data of wrong size");
    for (size_t i = 0; i < b64message.size(); ++i) {
        if (!valid_b64_char(b64message[i]))
            throw RunTimeError("Invalid BASE64 character");
        if (b64message[i] == '=') {
            if (i < b64message.size() - 2 ||
                (i == b64message.size() - 2 && b64message[i + 1] != '='))
            {
                throw RunTimeError("BASE64: Misplaced trailing =");
            }
        }
    }
}

std::string decode_base64(const std::string &b64message)
{
    if (!b64message.size())
        return std::string();
    check_base64(b64message);
    size_t decoded_size = b64_decoded_size(b64message);
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

void generate_random_bytes(void *buf, size_t len)
{
    int fd = ::open("/dev/urandom", O_RDONLY);
    if (fd == -1)
        throw RunTimeError("Can't open /dev/urandom");
    int n = ::read(fd, buf, len);
    ::close(fd);
    if (static_cast<int>(len) != n)
        throw RunTimeError("Can't read from /dev/urandom");
}

std::string generate_random_bytes(size_t length)
{
    std::string result(length, 0);
    generate_random_bytes(&result[0], result.size());
    return result;
}

std::string generate_random_number(size_t length)
{
    std::string rand_bytes = generate_random_bytes(length);
    std::string result(length, 0);
    for (size_t i = 0; i < length; ++i)
        result[i] = (((unsigned char)rand_bytes[i]) * 10 / 256) + '0';
    return result;
}

std::string generate_random_string(size_t length)
{
    std::string rand_bytes = generate_random_bytes(length);
    std::string result(length, 0);
    for (size_t i = 0; i < length; ++i) {
        int x = ((unsigned char)rand_bytes[i]) * (26*2 + 10) / 256;
        if (x < 26)
            result[i] = 'A' + x;
        else if (x < 26*2)
            result[i] = 'a' + (x - 26);
        else
            result[i] = '0' + (x - 26*2);
    }
    return result;
}

std::string generate_random_hex_string(size_t length)
{
    std::string rand_bytes = generate_random_bytes((length + 1) / 2);
    return string_to_hexstring(rand_bytes, HEX_NOSPACES).substr(0, length);
}

const std::string read_file(const std::string &file_name)
{
    std::ifstream inp(file_name.c_str());
    if (!inp)
        throw ::RunTimeError("can't open file: " + file_name);
    inp.seekg(0, std::ios::end);
    std::string result(inp.tellg(), ' ');
    inp.seekg(0, std::ios::beg);
    inp.read(&result[0], result.size());
    if (!inp)
        throw ::RunTimeError("can't read file: " + file_name);
    return result;
}

const std::string get_process_name()
{
    const std::string file_name = "/proc/self/cmdline";
    std::ifstream inp(file_name.c_str());
    if (!inp)
        throw ::RunTimeError("can't open file: " + file_name);
    std::string cmdline;
    std::copy(std::istream_iterator<char>(inp),
              std::istream_iterator<char>(),
              std::back_inserter(cmdline));
    size_t pos = cmdline.find('\0');
    if (std::string::npos != pos)
        cmdline = cmdline.substr(0, pos);
    pos = cmdline.rfind('/');
    if (std::string::npos != pos)
        cmdline = cmdline.substr(pos + 1);
    return cmdline;
}

const std::string fmt_string_escape(const std::string &s)
{
    std::string r;
    r.reserve(s.size() * 2);
    for (size_t pos = 0; pos < s.size(); ++pos) {
        if ('%' == s[pos])
            r += '%';
        r += s[pos];
    }
    return r;
}

// vim:ts=4:sts=4:sw=4:et:
