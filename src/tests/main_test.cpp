#include <iostream>
#include <vector>
#include <utility>

#include "../utils.h"
#include "../aes_crypter.h"


#define B64_TESTS      10
#define BINDEC_TESTS   20
#define AES_TESTS      10


std::string generate_random_number(const int length) {
    std::string result(length, 0);
    for(int i = 0; i < length; ++i) 
        result[i] = rand() % 10 + 48;
    return result;
}

std::string generate_random_string(const int length) {
    std::string result(length, 0);
    int tmp;
    for(int i = 0; i < length; ++i) {
        tmp = rand() % 62;
        if (tmp < 26) 
            result[i] = 65 + tmp;
        else if (tmp < 52)
            result[i] = 97 + (tmp - 26);
        else
            result[i] = 48 + (tmp - 52);
    }
    return result;
}
        
void test_base64_end_to_end() {
    std::cout << "test_base64_end_to_end" << std::endl;
    std::vector<std::string> messages;
    for(int i = 0; i < B64_TESTS; i++)
        messages.push_back(generate_random_string(rand() % 20 + 10));
    for (int i = 0; i < B64_TESTS ; ++i) {
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

void test_base64_encoding() {
    std::cout << "test_base64_encoding" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    //Argentina 
    //top100 football players
    messages.push_back(std::make_pair("GabrielBatistuta",   "R2FicmllbEJhdGlzdHV0YQ=="));
    messages.push_back(std::make_pair("HernanCrespo",       "SGVybmFuQ3Jlc3Bv"));
    messages.push_back(std::make_pair("AlfredoDiStefano",   "QWxmcmVkb0RpU3RlZmFubw=="));
    messages.push_back(std::make_pair("MarioKempes",        "TWFyaW9LZW1wZXM="));
    messages.push_back(std::make_pair("DiegoMaradona",      "RGllZ29NYXJhZG9uYQ=="));
    messages.push_back(std::make_pair("DanielPassarella",   "RGFuaWVsUGFzc2FyZWxsYQ=="));
    messages.push_back(std::make_pair("JavierSaviola",      "SmF2aWVyU2F2aW9sYQ=="));
    messages.push_back(std::make_pair("OmarSivori",         "T21hclNpdm9yaQ=="));
    messages.push_back(std::make_pair("JuanSebastianVeron", "SnVhblNlYmFzdGlhblZlcm9u"));
    messages.push_back(std::make_pair("JavierZanetti",      "SmF2aWVyWmFuZXR0aQ=="));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string encode64 = encode_base64(it->first);
        if(it->second.compare(encode64) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->first << " Encode: " << encode64 << std::endl;
        }
    }
}

void test_base64_decoding() {
    std::cout << "test_base64_decoding" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    //England 
    //top100 football players
    messages.push_back(std::make_pair("GordonBanks",    "R29yZG9uQmFua3M="));
    messages.push_back(std::make_pair("DavidBeckham",   "RGF2aWRCZWNraGFt"));
    messages.push_back(std::make_pair("BobbyCharlton",  "Qm9iYnlDaGFybHRvbg=="));
    messages.push_back(std::make_pair("KevinKeegan",    "S2V2aW5LZWVnYW4="));
    messages.push_back(std::make_pair("GaryLineker",    "R2FyeUxpbmVrZXI="));
    messages.push_back(std::make_pair("BobbyMoore",     "Qm9iYnlNb29yZQ=="));
    messages.push_back(std::make_pair("MichaelOwen",    "TWljaGFlbE93ZW4="));
    messages.push_back(std::make_pair("AlanShearer",    "QWxhblNoZWFyZXI="));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string decode64 = decode_base64(it->second);
        if(it->first.compare(decode64) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->second << " Decode: " << decode64 << std::endl;
        }
    }
}

void test_bindec_end_to_end() {
    std::cout << "test_bindec_end_to_end" << std::endl;
    std::vector<std::string> messages;
    BinDecConverter bindec_converter;
    for(int i = 0; i < BINDEC_TESTS; i++)
        messages.push_back(generate_random_number(rand() % 4 + 16)); //[16, 20]
    for(int i = 0; i < BINDEC_TESTS ; ++i) {
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
    //base64
    test_base64_encoding();
    test_base64_decoding();
    test_base64_end_to_end();

    //bindec
    test_bindec_end_to_end();
    
    //aes
    test_aes_coding();

    test_full_coding();
    test_dek_coding();
}
