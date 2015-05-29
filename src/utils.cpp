#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "utils.h"


BinDecConverter::BinDecConverter()
	: _terminator(0xF)
	, _mode(CYCLE_FORWARD)
{
}
BinDecConverter::BinDecConverter(const BinDecConverterFillMode mode)
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
                    if (t_index >= t_len)
                        t_index = t_index % t_len;
					t_value = in[t_index];
					break;
				case CYCLE_BACKWARD:
					t_index = t_len - (i - t_len);
                    if (t_index < 0)
                        t_index = t_len - 1 - ((t_index + 1) * -1) % t_len;
					t_value = in[t_index];
					break;
				case RANDOM:
					t_value = rand() % 10;
					break;
			}
		}
		if (i % 2 == 0)
			t_value <<= 4;
        else
            t_value &= 0xF;
		result[i / 2] |= t_value;
	}
	return result;
}

std::string BinDecConverter::decode(const std::string &in) {
	int blocks_cnt = in.size() / 16;
    int result_len = blocks_cnt * 2;
	std::string result;
	for (int i = 0; result_len; ++i) {
		unsigned char t_val = in[i / 2];
		if (i % 2 == 0)
			t_val >>= 4;
		t_val &= 0xF;
		if (t_val == this->_terminator)
			return result;
		result.push_back(char(48 + t_val));
	}
	return result;
}

std::string get_master_key() {
    return "12345678901234567890123456789012"; // 32 bytes
}

std::string string_to_bitstring(const std::string &input) {
    int in_size = input.size();
    const char *str = input.data();
    std::string result(in_size * 9 - 1, ' '); 
	for(int i = 0; i < in_size; ++i)
		for(int j = 0, k = 7; j < 8; ++j, --k) //hard logic
			result[i * 9 + j] = (str[i] >> k & 0x1) + 48;
    return result;
}

char int_to_hexchar(const int &val, const StringHexCaseMode mode = UPPERCASE) {
    if (val < 10)
        return 48 + val;
    else
        switch(mode) {
            case UPPERCASE:
                return 65 + (val - 10);
            case LOWERCASE:
                return 97 + (val - 10);
        }
    return -1;
}

int hexchar_to_int(const char &val) {
    if (val >= 48 && val <= 57)
        return val - 48;
    else if (val >= 65 && val <= 70)
        return val - 65 + 10;
    else if (val >= 97 && val <= 102)
        return val - 97 + 10;
    return -1;
}
 
std::string string_to_hexstring(const std::string &input) {
    int in_size = input.size();
    const char *str = input.data();
    std::string result(in_size * 3 - 1, ' '); 
	for(int i = 0; i < in_size; ++i) {
        result[i * 3]     = int_to_hexchar(str[i] >> 4 & 0xF);
        result[i * 3 + 1] = int_to_hexchar(str[i] & 0xF);
    }
    return result;
}

std::string string_from_hexstring(const std::string &hex_input) {
    int in_size = (hex_input.size() - (hex_input.size() / 3)) / 2;
    const char *str = hex_input.data();
    std::string result(in_size, 0); 
	for(int i = 0; i < in_size; ++i) {
        result[i] |= hexchar_to_int(str[i * 3]) << 4;
        result[i] |= hexchar_to_int(str[i * 3 + 1]);
    }
    return result;
}

std::string encode_base64(const std::string &message) {
	BIO *bio, *b64;
	FILE* stream;

    int size = message.size();
	int encoded_size = 4 * ceil((double)size / 3);
    std::string result(encoded_size, 0);
	char *buffer = &result[0];
    const char *c_message = message.data();

	stream = fmemopen(buffer, encoded_size + 1, "w");
	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new_fp(stream, BIO_NOCLOSE);
	bio = BIO_push(b64, bio);
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(bio, c_message, size);
	(void)BIO_flush(bio);
	BIO_free_all(bio);
	fclose(stream);

	return result;
}

int calc_decode_length(const std::string &b64input) {
	int padding = 0;
    int length = b64input.size();

	// Check for trailing '=''s as padding
	if(b64input[length - 1] == '=' && b64input[length - 2] == '=')
		padding = 2;
	else if (b64input[length - 1] == '=')
		padding = 1;

	return (int)length * 0.75 - padding;
}

std::string decode_base64(const std::string &b64message) {
	BIO *bio, *b64;

    int length = b64message.size();
	int decoded_length = calc_decode_length(b64message);
    std::string result(decoded_length, 0);
	char *buffer = &result[0];
    const char *c_b64message = b64message.data();

	FILE* stream = fmemopen((char*)c_b64message, length, "r");

	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new_fp(stream, BIO_NOCLOSE);
	bio = BIO_push(b64, bio);
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	decoded_length = BIO_read(bio, buffer, length);
	buffer[decoded_length] = '\0';

	BIO_free_all(bio);
	fclose(stream);

	return result; 
}

void convert_bits_to_ascii(unsigned char *input, int len) {
    for(int i = 0; i < len; ++i) {
        int tmp = input[i] % 64;
        if (tmp < 10)
            input[i] = tmp + 48;
        else if (tmp < 36)
            input[i] = (tmp - 10) + 65;
        else if (tmp < 62)
            input[i] = (tmp - 36) + 97;
        else if (tmp < 63)
            input[i] = 43;
        else
            input[i] = 47;
    }
}

std::string generate_dek_value(const int length) {
    unsigned char dek[length];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1)
        throw std::runtime_error("Can't open /dev/urandom");
    if (read(fd, &dek, sizeof(dek)) != static_cast<int>(sizeof(dek))) {
        close(fd);
        throw std::runtime_error("Can't read from /dev/urandom");
    }
    close(fd);
    convert_bits_to_ascii(dek, length);
    std::string result(length, 0);
    for(int i = 0; i < length; ++i)
        result[i] = dek[i];
    return result;
}

std::string generate_random_number(const int length) {
    std::string result(length, 0);
    for(int i = 0; i < length; ++i) 
        result[i] = rand() % 10 + 48;
    return result;
}

std::string generate_random_string(const int length) {
    std::string result(length, 0);
    for(int i = 0; i < length; ++i) {
        int tmp = rand() % 62;
        if (tmp < 26) 
            result[i] = 65 + tmp;
        else if (tmp < 52)
            result[i] = 97 + (tmp - 26);
        else
            result[i] = 48 + (tmp - 52);
    }
    return result;
}

