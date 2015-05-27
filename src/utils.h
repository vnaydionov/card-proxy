#ifndef UTILS_H
#define UTILS_H
#include <bitset>
#include <string>
#include <math.h>
#include <openssl/bio.h>
#include <openssl/evp.h>


enum BinDecConverterFillMode {
	ZERO,
	CYCLE_FORWARD,
	CYCLE_BACKWARD,
	RANDOM
};

enum StringHexCaseMode {
	UPPERCASE,
    LOWERCASE,
};

enum StringHexSplitMode {
    NONE,
    WHITESPACE,
};

int calc_decode_length(const std::string &b64input, const int length);
std::string encode_base64(const std::string &message);
std::string decode_base64(const std::string &b64message);

std::string string_to_bitstring(const std::string &input);
std::string string_from_bitstring(const std::string &bit_input);

std::string string_to_hexstring(const std::string &input);
std::string string_from_hexstring(const std::string &hex_input);

std::string generate_dek_value(const int length = 32);
std::string get_master_key();

std::string generate_random_number(const int length);
std::string generate_random_string(const int length);

class BinDecConverter {
public:
	BinDecConverter();
	BinDecConverter(const BinDecConverterFillMode mode);

	std::string encode(const std::string &in);
	std::string decode(const std::string &in);

	void set_mode(const BinDecConverterFillMode mode);
	BinDecConverterFillMode get_mode();

private:
	int _terminator;
	BinDecConverterFillMode _mode;
};



#endif
