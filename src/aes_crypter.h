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
    void set_master_key(const std::string &key);

private:
	const int _key_size;
	const int _block_size;
	const int _block_size_bit;
	AES_KEY _encrypt_key, _decrypt_key;

};

class AESBlockSizeException {
public:
    AESBlockSizeException();
    AESBlockSizeException(const int &size, const std::string &val);
    virtual ~AESBlockSizeException();

    std::string get_string();
    int get_block_size();

private:
    std::string value;
    int block_size;
};

class AESEncryptBlockSizeException : public AESBlockSizeException {
};

class AESDecryptBlockSizeException : public AESBlockSizeException {
};
#endif /* AESCRYPTER_H_ */
