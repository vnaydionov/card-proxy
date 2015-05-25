#ifndef UTILS_H
#define UTILS_H
#include <bitset>
#include <string>
#include <math.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

//#include <orm/data_object.h>
//#include <orm/domain_object.h>

//#include "domain/DataKey.h"

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

struct DEKPoolStatus {
    long total_count;
    long active_count;
    long use_count;
};

int calc_decode_length(const std::string &b64input, const int length);
std::string encode_base64(const std::string &message);
std::string decode_base64(const std::string &b64message);

std::string string_to_bitstring(const std::string &input);
std::string string_from_bitstring(const std::string &bit_input);

std::string string_to_hexstring(const std::string &input);
std::string string_from_hexstring(const std::string &hex_input);

std::string get_master_key();
//Domain::DataKey get_active_dek(Yb::Session &session);
std::string generate_dek_value();
//DEKPoolStatus get_dek_pool_status(Yb::Session &session);

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
