#include <iostream>

#include "aes_crypter.h"
#include "utils.h"


AESCrypter::AESCrypter()
	: _key_size(256)
	, _block_size(16)
	, _block_size_bit(128)
{
}

AESCrypter::AESCrypter(const std::string &key)
	: _key_size(256)
	, _block_size(16)
	, _block_size_bit(128)
{
	auto *_key = reinterpret_cast<const unsigned char *>(key.data());
	AES_set_encrypt_key(_key, _key_size, &_encrypt_key);
	AES_set_decrypt_key(_key, _key_size, &_decrypt_key);
}

void AESCrypter::set_master_key(const std::string &key) {
	auto *_key = reinterpret_cast<const unsigned char *>(key.data());
	AES_set_encrypt_key(_key, _key_size, &_encrypt_key);
	AES_set_decrypt_key(_key, _key_size, &_decrypt_key);
}

std::string AESCrypter::encrypt(const std::string &input_text) {
	if(input_text.size() == 0 || input_text.size() % _block_size != 0) 
        throw AESBlockSizeException(_block_size, input_text);
	int blocks = input_text.size() / _block_size;
	const unsigned char *input_text_char =
        reinterpret_cast<const unsigned char *>(input_text.data());
	std::string result(blocks * _block_size, 0);
    unsigned char *cipher_block =
        reinterpret_cast<unsigned char *>(&result[0]);
	for(int i = 0; i < blocks; ++i) {
		int mas_pos = i * _block_size;
		AES_encrypt(&input_text_char[mas_pos], &cipher_block[mas_pos], &_encrypt_key);
	}
	return result;
}

std::string AESCrypter::decrypt(const std::string &input_cipher) {
	if(input_cipher.size() == 0 || input_cipher.size() % _block_size != 0) 
        throw AESBlockSizeException(_block_size, input_cipher);
	int blocks = input_cipher.size() / _block_size;
	const unsigned char *input_cipher_char =
        reinterpret_cast<const unsigned char *>(input_cipher.data());
    std::string result(blocks * _block_size, 0);
    unsigned char *text_block =
        reinterpret_cast<unsigned char *>(&result[0]);
	for(int i = 0; i < blocks; ++i) {
		int mas_pos = i * _block_size;
		AES_decrypt(&input_cipher_char[mas_pos], &text_block[mas_pos], &_decrypt_key);
	}
	return result;
}


AESCrypter::~AESCrypter() {
}

AESBlockSizeException::AESBlockSizeException()
    : block_size(0) {
}

AESBlockSizeException::AESBlockSizeException(const int &size,
    const std::string &val)
    : value(val), block_size(size) {
}

AESBlockSizeException::~AESBlockSizeException() {
}

std::string AESBlockSizeException::get_string() {
    return value;
}

int AESBlockSizeException::get_block_size() {
    return block_size;
}

std::string AESBlockSizeException::to_string() {
    std::string result = "Invalid block size: " + std::to_string(value.size())
            + ", expected %" + std::to_string(block_size) 
            + " [" + string_to_hexstring(value) + "]";
    return result;
}

