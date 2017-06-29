// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__UTILS_H
#define CARD_PROXY__UTILS_H

#include <string>
#include <stdexcept>

class RunTimeError: public std::runtime_error
{
public:
    RunTimeError(const std::string &msg);
};

enum StringHexCaseMode { HEX_UPPERCASE=0, HEX_LOWERCASE=1, HEX_NOSPACES=2 };

char int_to_hexchar(int digit, int mode=0);
int hexchar_to_int(char symbol);

std::string string_to_hexstring(const std::string &input, int mode=0);
std::string string_from_hexstring(const std::string &hex_input, int mode=0);

std::string encode_base64(const std::string &message);
std::string decode_base64(const std::string &b64message);

void generate_random_bytes(void *buf, size_t len);
std::string generate_random_bytes(size_t length);

std::string generate_random_number(size_t length);
std::string generate_random_string(size_t length);
std::string generate_random_hex_string(size_t length);

const std::string read_file(const std::string &file_name);
const std::string get_process_name();
const std::string fmt_string_escape(const std::string &s);
const std::string replace_str(const std::string &str,
                              const std::string &search,
                              const std::string &replace);
const std::string get_utc_iso_ts();

#endif // CARD_PROXY__UTILS_H
// vim:ts=4:sts=4:sw=4:et:
