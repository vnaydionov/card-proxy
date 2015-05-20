#include <iostream>
#include <openssl/aes.h>

#include "card_proxy.h"
#include "utils.h"
#include "aes_crypter.h"

CardProxy::CardProxy(const std::string &key) {
	_key = key; //"abcdefghijklmnop"; //generate_key
}

CardProxy::~CardProxy() {
}

std::string CardProxy::get_token(const CardData &card_data) {
	//mask pan
	//lookup pair masked_pan+exdate in db
	//return token if find smthng

	//else
	AESCrypter aes_crypter(this->_key);
	std::string bindec_pan = BinDecConverter().encode(card_data.pan);
	std::string cripted_pan = aes_crypter.encrypt(bindec_pan);
	std::string base64_pan = Base64Converter().encode_base64(cripted_pan);
	//write to db
	return base64_pan;
}

Card CardProxy::get_data(const std::string &token) {
	//find dataset by token;
	std::string cripted_pan = Base64Converter().decode_base64(token);
	std::string decrypted_pan = AESCrypter(this->_key).decrypt(cripted_pan);
	std::string pan = BinDecConverter().decode(decrypted_pan);
	Card card = Card("", "", pan, "");
	return card;
}
