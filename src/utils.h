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

enum StringHexMode {
	UPPERCASE,
    LOWERCASE,
};

int calc_decode_length(const std::string &b64input, const int length);
std::string encode_base64(const std::string &message);
std::string decode_base64(const std::string &b64message);

std::string string_to_bitstring(const std::string &input);
std::string string_to_hexstring(const std::string &input);

std::string get_master_key();
std::string generate_dek();

class BinDecConverter {
public:
	BinDecConverter();
	BinDecConverter(BinDecConverterFillMode mode);

	std::string encode(const std::string &in);
	std::string decode(const std::string &in);

    //template for bitset size ???
	std::bitset<128> encode_bitset(const std::string &in);
	std::string decode_bitset(const std::bitset<128> &in);

	void set_mode(BinDecConverterFillMode mode);
	BinDecConverterFillMode get_mode();

private:
	int _terminator;
	BinDecConverterFillMode _mode;
};



#endif
