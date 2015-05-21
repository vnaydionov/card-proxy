#ifndef AESCRYPTER_H_
#define AESCRYPTER_H_
#include <openssl/aes.h>

class AESCrypter {
public:
	AESCrypter();
	AESCrypter(const std::string &key);
	AESCrypter(const unsigned char *key);
	virtual ~AESCrypter();

	std::string encrypt(const std::string &in_text);
	std::string decrypt(const std::string &in_cipher);

private:
	int _key_size;
	int _block_size;
	AES_KEY encrypt_key, decrypt_key;

};

#endif /* AESCRYPTER_H_ */
