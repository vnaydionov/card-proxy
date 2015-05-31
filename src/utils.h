// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef CARD_PROXY__UTILS_H
#define CARD_PROXY__UTILS_H

#include <string>

enum StringHexCaseMode { HEX_UPPERCASE=0, HEX_LOWERCASE=1, HEX_NOSPACES=2 };

char int_to_hexchar(int digit, int mode=0);
int hexchar_to_int(char symbol);

std::string string_to_hexstring(const std::string &input, int mode=0);
std::string string_from_hexstring(const std::string &hex_input, int mode=0);

std::string encode_base64(const std::string &message);
std::string decode_base64(const std::string &b64message);

std::string bcd_decode(const std::string &bcd_input);
std::string bcd_encode(const std::string &ascii_input);

std::string generate_random_bytes(size_t length);

// in fact: pseudo-random, for testing only
std::string generate_random_number(size_t length);
std::string generate_random_string(size_t length);

std::string mask_pan(const std::string &pan);

#endif // CARD_PROXY__UTILS_H
// vim:ts=4:sts=4:sw=4:et:
