#include <iostream>

#include "aes_crypter.h"


AESCrypter::AESCrypter()
    : _key_size(256)
    , _block_size(128)
{
}

AESCrypter::AESCrypter(const std::string &key)
    : _key_size(256)
    , _block_size(128)
{
	const unsigned char *_key = (unsigned char*)key.c_str();
	AES_set_encrypt_key(_key, this->_key_size, &this->encrypt_key);
	AES_set_decrypt_key(_key, this->_key_size, &this->decrypt_key);
}

AESCrypter::AESCrypter(const unsigned char *key)
    : _key_size(256)
    , _block_size(128)
{
	AES_set_encrypt_key(key, this->_key_size, &this->encrypt_key);
	AES_set_decrypt_key(key, this->_key_size, &this->decrypt_key);
}

std::string AESCrypter::encrypt(const std::string &in_text) {
	int mas_pos;
	int blocks = in_text.size() % 16 == 0 ? in_text.size() / 16
										  : in_text.size() / 16 + 1;
	// if % 16 != 0 add some trash
	std::string result;
	std::cout << in_text.c_str() << std::endl;
	unsigned char *in_text_char = (unsigned char*)in_text.c_str();
	unsigned char *cipher_block = new unsigned char[16 * blocks];
	for(int i = 0; i < blocks; ++i) {
		mas_pos = i * 16;
		AES_encrypt(&in_text_char[mas_pos], &cipher_block[mas_pos], &this->encrypt_key);
	}
	//cipher_block[mas_pos + this->_block_size / 8 + 1] = '\n';
	result = std::string((char*) cipher_block);
	return result;
}

std::string AESCrypter::decrypt(const std::string &input_cipher) {
	int mas_pos, blocks = input_cipher.size() / 16; //change for base64
	std::string result;
	unsigned char *input_cipher_char = (unsigned char*)input_cipher.c_str();
	unsigned char *text_block = new unsigned char[16 * blocks];
	for(int i = 0; i < blocks; ++i) {
		mas_pos = i * 16;
		AES_decrypt(&input_cipher_char[mas_pos], &text_block[mas_pos], &this->decrypt_key);
	}
	//text_block[mas_pos + this->_block_size / 8 + 1] = '\n';
	result = std::string((char*) text_block);
	return result;
}


AESCrypter::~AESCrypter() {
	// TODO Auto-generated destructor stub
}

