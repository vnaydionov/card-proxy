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
        std::string encode64 = encode_base64(messages[i]);
        std::string decode64 = decode_base64(encode64);
        if(messages[i].compare(decode64) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << messages[i] << " Decode: " << decode64 << std::endl;
        }
    }
}

void test_bindec_convert() {
    std::string messages[] = {
        "1234567891234567",
        "1231241435984941",
        "12312312312312432",
        "334234588778685123",
        "87982634723692523942",
    };
    int len = sizeof(messages) / sizeof(messages[0]);
    BinDecConverter bindec_converter;

    std::cout << "test_bindec_convert" << std::endl;
    for (int i = 0; i < len; ++i) {
        std::string encode_bindec = bindec_converter.encode(messages[i]);
        std::string decode_bindec = bindec_converter.decode(encode_bindec);
        if(messages[i].compare(decode_bindec) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << messages[i] << " Decode: " << decode_bindec << std::endl;
        }
    }
}


void test_aes_coding() {
    std::string messages[] = {
        "1234567891234567",
        "1231241435984941",
        "dadjahkjadflajdh",
        "gjhfdghsdfgahoad"
    };
    int len = sizeof(messages) / sizeof(messages[0]);
    std::string key = "123456789012345";

    std::cout << "test_aes_coding" << std::endl;
    AESCrypter aes_crypter(key);
    for(int i = 0; i < len; ++i) {
        try {   
            std::string encode_aes = aes_crypter.encrypt(messages[i]);
            std::string decode_aes = aes_crypter.decrypt(encode_aes);
            if (messages[i].compare(decode_aes) == 0)
                std::cout << "Result:    Success!" << std::endl;
            else {
                std::cout << "Result:    Fail!" << std::endl;
                std::cout << "Input: " << messages[i] << " Decode: " << decode_aes << std::endl;
            }
        } catch(std::string err) {
            std::cout << err << std::endl;
        }
    }
}
            
void test_full_coding() {
    std::string messages[] = {
        "1234567891234567",
        "1231241435984941",
        "12312312312312432",
        "334234588778685123",
        "87982634723692523942",
    };
    int len = sizeof(messages) / sizeof(messages[0]);
    std::string key = "123456789012345";

    std::cout << "test_full_coding" << std::endl;
    AESCrypter aes_crypter(key);
    BinDecConverter bindec_convert;
    for(int i = 0; i < len; ++i) {
        try {
            std::string encode_bindec = bindec_convert.encode(messages[i]);
            std::string encode_aes = aes_crypter.encrypt(encode_bindec);
            std::string encode_b64 = encode_base64(encode_aes);
            std::string decode_b64 = decode_base64(encode_b64);
            std::string decode_aes = aes_crypter.decrypt(decode_b64);
            std::string decode_bindec = bindec_convert.decode(decode_aes); 
            if (messages[i].compare(decode_bindec) == 0)
                std::cout << "Result:    Success!" << std::endl;
            else {
                std::cout << "Result:    Fail!" << std::endl;
                std::cout << "Input: " << messages[i] << " Decode: " << decode_aes << "." << std::endl;
            }
        } catch(std::string err) {
            std::cout << err << std::endl;
        }
    }
}

void test_dek_coding() {
    std::string dek[] = {
        "12345678901234567890123456789012", //32
        "12312414359849413434513451242346",
        "88946319839517858134581732839258",
    };
    int len = sizeof(dek) / sizeof(dek[0]);
    std::string key = "123456789012345";
    
    std::cout << "test_dek_coding" << std::endl;
    AESCrypter aes_crypter(key);
    for(int i = 0; i < len; ++i) {
        try {
            std::string encode_dek = aes_crypter.encrypt(dek[i]);
            std::string decode_dek = aes_crypter.decrypt(encode_dek);
            if (dek[i].compare(decode_dek) == 0)
                std::cout << "Result:    Success!" << std::endl;
            else {
                std::cout << "Result:    Fail!" << std::endl;
                std::cout << "Input: " << dek[i] << " Decode: " << decode_dek << "." << std::endl;
            }
        } catch(std::string err) {
            std::cout << err << std::endl;
        }
    }
}

    
//TODO: random data generation
//TODO: unittesting
//TODO: add predicted tests
int main() {
    test_bindec_convert();
    test_base64_coding();
    test_aes_coding();
    test_full_coding();
    test_dek_coding();
}
