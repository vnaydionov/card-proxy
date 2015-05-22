#include <iostream>
#include "../utils.h"
#include "../aes_crypter.h"

void test_base64_coding() {
    std::string messages[] = {
        "1234567891234567",
        "1231241435984941",
        "12312312312312432",
        "334234588778685123",
        "87982634723692523942",
    };
    int len = sizeof(messages) / sizeof(messages[0]);

    std::cout << "test_base64_coding" << std::endl;
    for (int i = 0; i < len; ++i) {
        std::string encode64 = encode_base64(messages[i], messages[i].length());
        std::string decode64 = decode_base64(encode64, encode64.length());
        if(messages[i].compare(decode64) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << messages[i] << " Decode: " << decode64 << std::endl;
        }
    }
}

void test_aes_coding() {
    std::string messages[] = {
        "1234567891234567",
        "1231241435984941",
        "12312312312312432",
        "334234588778685123",
        "87982634723692523942",
    };
    int len = sizeof(messages) / sizeof(messages[0]);
    std::string key = "123456789012345";

    std::cout << "test_aes_coding" << std::endl;
    AESCrypter aes_crypter(key);
    for(int i = 0; i < len; ++i) {
        std::string encode_aes = aes_crypter.encrypt(messages[i]);
        std::string decode_aes = aes_crypter.decrypt(encode_aes);
        if (messages[i].compare(decode_aes) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << messages[i] << " Decode: " << decode_aes << std::endl;
        }
    }
}
            
void test_aes_base64_coding() {
    std::string messages[] = {
        "1234567891234567",
        "1231241435984941",
        "12312312312312432",
        "334234588778685123",
        "87982634723692523942",
    };
    int len = sizeof(messages) / sizeof(messages[0]);
    std::string key = "123456789012345";

    std::cout << "test_aes_base64_coding" << std::endl;
    AESCrypter aes_crypter(key);
    for(int i = 0; i < len; ++i) {
        std::string encode_aes = aes_crypter.encrypt(messages[i]);
        std::string encode_b64 = encode_base64(encode_aes, encode_aes.length());
        std::string decode_b64 = decode_base64(encode_b64, encode_b64.length());
        std::string decode_aes = aes_crypter.decrypt(decode_b64);
        if (messages[i].compare(decode_aes) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << messages[i] << " Decode: " << decode_aes << "." << std::endl;
        }
    }
}
    

int main() {
    test_base64_coding();
    test_aes_coding();
    test_aes_base64_coding();
}
