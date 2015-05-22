#ifndef AESCRYPTER_H_
#define AESCRYPTER_H_
#include <openssl/aes.h>

class AESCrypter {
public:
	AESCrypter();
	AESCrypter(const std::string &key);
	AESCrypter(const unsigned char *key);
	virtual ~AESCrypter();

	std::string encrypt(const std::string &input_text);
	std::string decrypt(const std::string &input_cipher);

private:
	const int _key_size;
	const int _block_size;
	const int _block_size_bit;
	AES_KEY _encrypt_key, _decrypt_key;

};

#endif /* AESCRYPTER_H_ */
