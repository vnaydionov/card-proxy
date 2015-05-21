#ifndef UTILS_H
#define UTILS_H
#include <bitset>
#include <string>
#include <openssl/bio.h>
#include <openssl/evp.h>

enum BinDecConverterFillMode {
	ZERO,
	CYCLE_FORWARD,
	CYCLE_BACKWARD,
	RANDOM
};

class Base64Converter {
public:
	std::string encode_base64(const std::string &input);
	std::string decode_base64(const std::string &input);
};

//template for bitset size
class BinDecConverter {
public:
	BinDecConverter();
	BinDecConverter(BinDecConverterFillMode mode);

	std::string encode(const std::string &in);
	std::string decode(const std::string &in);

	std::bitset<128> encode_bitset(const std::string &in);
	std::string decode_bitset(const std::bitset<128> &in);

	void set_mode(BinDecConverterFillMode mode);
	BinDecConverterFillMode get_mode();

private:
	int _terminator;
	BinDecConverterFillMode _mode;
};



#endif
