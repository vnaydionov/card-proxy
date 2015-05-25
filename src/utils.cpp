#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "utils.h"

//using namespace Domain;

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
    std::string result(in_size * 9 - 1, ' '); 
	for(int i = 0; i < in_size; ++i)
		for(int j = 0, k = 7; j < 8; ++j, --k) //hard logic
			result[i * 9 + j] = (str[i] >> k & 0x1) + 48;
    return result;
}

char int_to_hexchar(const int &val, const StringHexMode mode = UPPERCASE) {
    if (val < 10)
        return 48 + val;
    else
        switch(mode) {
            case UPPERCASE:
                return 65 + (val - 10);
            case LOWERCASE:
                return 97 + (val - 10);
        }
}

int hexchar_to_int(const char &val) {
    if (val >= 48 && val <= 57)
        return val - 48;
    else if (val >= 65 && val <= 70)
        return val - 65 + 10;
    else if (val >= 97 && val <= 102)
        return val - 97 + 10;
    //error
    return -1;
}
 
std::string string_to_hexstring(const std::string &input) {
    int in_size = input.size();
    const char *str = input.c_str();
    std::string result(in_size * 3 - 1, ' '); 
	for(int i = 0; i < in_size; ++i) {
        result[i * 3]     = int_to_hexchar(str[i] >> 4 & 0xF);
        result[i * 3 + 1] = int_to_hexchar(str[i] & 0xF);
    }
    return result;
}

std::string string_from_hexstring(const std::string &hex_input) {
    int in_size = hex_input.size() - (hex_input.size() / 3);
    const char *str = hex_input.c_str();
    std::string result(in_size, 0); 
	for(int i = 0; i < in_size; ++i) {
        result[i] |= hexchar_to_int(str[i * 3]) << 4;
        result[i] |= hexchar_to_int(str[i * 3 + 1]);
    }
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

std::string get_master_key() {
    // make there UBER LOGIC FOR COMPOSE MASTER KEY
    return "12345678901234567890123456789012"; // 32 bytes
}

void convert_bits_to_ascii(unsigned char *input, int len) {
    int tmp;
    for(int i = 0; i < len; ++i) {
        tmp = input[i] % 64;
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

//DEKPoolStatus get_dek_pool_status(Yb::Session &session) {
//    DEKPoolStatus dek_status;
//    dek_status.total_count= Yb::query<DataKey>(session).count();
//    dek_status.active_count = Yb::query<DataKey>(session)
//            .filter_by(DataKey::c.counter < 10).count();
//    dek_status.use_count = 0;
//    return dek_status;
//}

//void generate_new_dek(Yb::Session &session) {
//    Domain::DataKey data_key;
//    std::string dek_value = generate_dek_value();
//}

//Domain::DataKey get_active_dek(Yb::Session &session) {
//    DEKPoolStatus dek_status = get_dek_pool_status(session);
//    while(dek_status.active_count < 10) { 
//        generate_new_dek(session);
//        dek_status = get_dek_pool_status(session);
//    }
//
//    DataKey dek = Yb::query<DataKey>(session)
//            .filter_by(DataKey::c.counter < 10)
//            .order_by(DataKey::c.counter)
//            .one();
//    return dek;
//}


std::string generate_dek_value() {
    unsigned char dek[32];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1)
        throw std::runtime_error("Can't open /dev/urandom");
    if (read(fd, &dek, sizeof(dek)) != sizeof(dek)) {
        close(fd);
        throw std::runtime_error("Can't read from /dev/urandom");
    }
    close(fd);
    convert_bits_to_ascii(dek, 32);
    return std::string((char*) dek);
}

