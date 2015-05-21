#include <algorithm>
#include <iostream>

#include "utils.h"

BinDecConverter::BinDecConverter() {
	this->_mode = CYCLE_FORWARD;
}

BinDecConverter::BinDecConverter(BinDecConverterFillMode mode) {
	this->_mode = mode;
}

std::string BinDecConverter::encode(const std::string &in) {
	int t_len = in.length();
	int blocks_cnt = (t_len - 1) % 32 == 0 ? t_len / 32 : t_len / 32 + 1;
	int result_len = blocks_cnt * 16;
	std::string result(result_len, 0);
	for(int i = 0; i < result_len * 2; ++i) {
		unsigned t_value;
		if (i < t_len)
			t_value = (in[i] - 48);
		else if (i == t_len)
			t_value = this->_terminator;
		else {
			int t_index;
			switch(this->_mode) {
				case ZERO:
					t_value = 0x0;
					break;
				case CYCLE_FORWARD:
					t_index = i - t_len - 1;
					t_value = in[t_index];
					break;
				case CYCLE_BACKWARD:
					t_index = t_len - (i - t_len);
					t_value = in[t_index];
					break;
				case RANDOM:
					t_value = rand() % 10;
					break;
			}
		}
		if (i % 2 == 0)
			t_value <<= 4;
		result[i / 2] |= t_value;
	}
	return result;
}

std::bitset<128> BinDecConverter::encode_bitset(const std::string &in) {
	std::string bits = this->encode(in);
	std::bitset<128> result;
	for(int i = 0; i < 16; ++i) {
		for(int j = 0; j < 8; ++j) {
			int index = 128 - (i + 1) * 8 + j;
			result[index] = bits[i] >> j & 0x1;
		}
	}
	return result;
}

std::string BinDecConverter::decode(const std::string &in) {
	int blocks_cnt = in.size() / 16, result_len = blocks_cnt * 2;
	std::string result;
	for (int i = 0; result_len; ++i) {
		char t_val = in[i / 2];
		if (i % 2 == 0)
			t_val >>= 4;
		t_val &= 0xF;
		if (t_val == this->_terminator)
			return result;
		result.push_back(char(48 + t_val));
	}
	return result;
}

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";


static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string Base64Converter::encode_base64(const std::string &input) {
	std::string ret;
	int i = 0;
	int j = 0;
	const char *bytes_to_encode = input.c_str();
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];
	int in_len = input.size();
	std::cout << "encode len: " << in_len << std::endl;

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
	    if (i == 3) {
	    	char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
	    	char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
	    	char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
	    	char_array_4[3] = char_array_3[2] & 0x3f;
	    	for(i = 0; (i <4) ; i++)
	    		ret += base64_chars[char_array_4[i]];
	    	i = 0;
	    }
	}

	if (i) {
		for(j = i; j < 3; j++)
			char_array_3[j] = '\0';
		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;
		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];
		while((i++ < 3))
			ret += '=';
	}
	return ret;
}

std::string Base64Converter::decode_base64(const std::string &encoded_input) {
	size_t in_len = encoded_input.size();
	size_t i = 0;
	size_t j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while (in_len-- && ( encoded_input[in_] != '=') && is_base64(encoded_input[in_])) {
		char_array_4[i++] = encoded_input[in_]; in_++;
		if (i ==4) {
			for (i = 0; i <4; i++)
				char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
		for (j = 0; (j < i - 1); j++)
			ret += char_array_3[j];
	}

	return ret;
}
