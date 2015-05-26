#include <iostream>

#include "aes_crypter.h"
#include "utils.h"


AESCrypter::AESCrypter()
	: _key_size(256)
	, _block_size_bit(128)
	, _block_size(16)
{
}

AESCrypter::AESCrypter(const std::string &key)
	: _key_size(256)
	, _block_size_bit(128)
	, _block_size(16)
{
	const unsigned char *_key = (unsigned char*)key.c_str();
	AES_set_encrypt_key(_key, _key_size, &_encrypt_key);
	AES_set_decrypt_key(_key, _key_size, &_decrypt_key);
}

AESCrypter::AESCrypter(const unsigned char *key)
	: _key_size(256)
	, _block_size_bit(128)
	, _block_size(16)
{
	AES_set_encrypt_key(key, _key_size, &_encrypt_key);
	AES_set_decrypt_key(key, _key_size, &_decrypt_key);
}

void AESCrypter::set_master_key(const std::string &key) {
	const unsigned char *_key = (unsigned char*)key.c_str();
	AES_set_encrypt_key(_key, _key_size, &_encrypt_key);
	AES_set_decrypt_key(_key, _key_size, &_decrypt_key);
}

std::string AESCrypter::encrypt(const std::string &input_text) {
	if(input_text.size() == 0 || input_text.size() % _block_size != 0) 
        throw AESBlockSizeException(_block_size, input_text);
	int blocks = input_text.size() / _block_size;
	unsigned char *input_text_char = (unsigned char*)input_text.c_str();
	unsigned char *cipher_block = new unsigned char[blocks * _block_size];
	for(int i = 0, mas_pos; i < blocks; ++i) {
		mas_pos = i * _block_size;
		AES_encrypt(&input_text_char[mas_pos], &cipher_block[mas_pos], &_encrypt_key);
	}
	std::string result(blocks * _block_size, 0);
	for(int i = 0; i < blocks * _block_size; ++i)
		result[i] = cipher_block[i];
	return result;
}

std::string AESCrypter::decrypt(const std::string &input_cipher) {
	if(input_cipher.size() == 0 || input_cipher.size() % _block_size != 0) 
        throw AESBlockSizeException(_block_size, input_cipher);
	int blocks = input_cipher.size() / _block_size;
	unsigned char *input_cipher_char = (unsigned char*)input_cipher.c_str();
	unsigned char *text_block = new unsigned char[blocks * _block_size];
	for(int i = 0, mas_pos; i < blocks; ++i) {
		mas_pos = i * _block_size;
		AES_decrypt(&input_cipher_char[mas_pos], &text_block[mas_pos], &_decrypt_key);
	}
	std::string result(blocks * _block_size, 0);
	for(int i = 0; i < blocks * _block_size; ++i) 
		result[i] = text_block[i];
	return result;
}


AESCrypter::~AESCrypter() {
}

AESBlockSizeException::AESBlockSizeException() : block_size(0), value("") {
}

AESBlockSizeException::AESBlockSizeException(const int &size ,const std::string &val) : block_size(size), value(val) {
}

AESBlockSizeException::~AESBlockSizeException() {
}

std::string AESBlockSizeException::get_string() {
    return this->value;
}

int AESBlockSizeException::get_block_size() {
    return this->block_size;
}

