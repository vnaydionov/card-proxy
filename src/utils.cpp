#include <algorithm>
#include <iostream>

#include "utils.h"

BinDecConverter::BinDecConverter()
	: _terminator(0xF)
	, _mode(CYCLE_FORWARD)
{
}

BinDecConverter::BinDecConverter(BinDecConverterFillMode mode)
	: _terminator(0xF)
	, _mode(mode)
{
}

std::string BinDecConverter::encode(const std::string &in) {
	int t_len = in.length();
	// text len + terminator
	int blocks_cnt = (t_len + 1) % 32 == 0 ? t_len / 32 : t_len / 32 + 1;
	int result_len = blocks_cnt * 16;
	std::string result(result_len, 0);
	for(int i = 0; i < result_len * 2; ++i) {
		unsigned t_value;
		if (i < t_len) // check isdigit
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
	for(int i = 0; i < 16; ++i)
		for(int j = 0; j < 8; ++j) {
			int index = 128 - (i + 1) * 8 + j;
			result[index] = bits[i] >> j & 0x1;
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

std::string string_to_bitstring(const std::string &input) {
    int in_size = input.size();
    const char *str = input.c_str();
    std::string result(in_size * 8, 0); 
	for(int i = 0; i < in_size; ++i)
		for(int j = 0; j < 8; ++j)
			result[i * 8 + j] = (str[i] >> j & 0x1) + 48;
    return result;
}

std::string encode_base64(const std::string &message) {
	BIO *bio;
	BIO *b64;
	FILE* stream;

    int length = message.length();
	int encoded_size = 4 * ceil((double)length / 3);
	char *buffer = new char[encoded_size + 1]; //memleak?
    const char *c_message = message.c_str();

	stream = fmemopen(buffer, encoded_size + 1, "w");
	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new_fp(stream, BIO_NOCLOSE);
	bio = BIO_push(b64, bio);
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(bio, c_message, length);
	(void)BIO_flush(bio);
	BIO_free_all(bio);
	fclose(stream);

	return std::string(buffer);
}

int calc_decode_length(const std::string &b64input) {
	int padding = 0;
    int length = b64input.length();

	// Check for trailing '=''s as padding
	if(b64input[length - 1] == '=' && b64input[length - 2] == '=')
		padding = 2;
	else if (b64input[length - 1] == '=')
		padding = 1;

	return (int)length * 0.75 - padding;
}

std::string decode_base64(const std::string &b64message) {
	BIO *bio;
	BIO *b64;

    int length = b64message.length();
	int decoded_length = calc_decode_length(b64message);
	char *buffer = new char[decoded_length + 1];
    const char *c_b64message = b64message.c_str();

	FILE* stream = fmemopen((char*)c_b64message, length, "r");

	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new_fp(stream, BIO_NOCLOSE);
	bio = BIO_push(b64, bio);
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	decoded_length = BIO_read(bio, buffer, length);
	buffer[decoded_length] = '\0';

	BIO_free_all(bio);
	fclose(stream);

	return std::string(buffer); 
}
