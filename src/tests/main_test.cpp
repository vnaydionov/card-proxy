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

void test_bindec_end_to_end_zero() {
    std::cout << "test_bindec_end_to_end_zero" << std::endl;
    std::vector<std::string> messages;
    BinDecConverter bindec_converter = BinDecConverter(ZERO);
    for(int i = 0; i < BINDEC_TESTS; i++)
        messages.push_back(generate_random_number(rand() % 20 + 10)); //[10, 30]
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

void test_bindec_end_to_end_cycle_forward() {
    std::cout << "test_bindec_end_to_end_cycle_forward" << std::endl;
    std::vector<std::string> messages;
    BinDecConverter bindec_converter = BinDecConverter(CYCLE_FORWARD);
    for(int i = 0; i < BINDEC_TESTS; i++)
        messages.push_back(generate_random_number(rand() % 20 + 10)); //[10, 30]
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

void test_bindec_end_to_end_cycle_backward() {
    std::cout << "test_bindec_end_to_end_cycle_backward" << std::endl;
    std::vector<std::string> messages;
    BinDecConverter bindec_converter = BinDecConverter(CYCLE_BACKWARD);
    for(int i = 0; i < BINDEC_TESTS; i++)
        messages.push_back(generate_random_number(rand() % 20 + 10)); //[10, 30]
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

void test_bindec_end_to_end_random() {
    std::cout << "test_bindec_end_to_end_random" << std::endl;
    std::vector<std::string> messages;
    BinDecConverter bindec_converter = BinDecConverter(RANDOM);
    for(int i = 0; i < BINDEC_TESTS; i++)
        messages.push_back(generate_random_number(rand() % 20 + 10)); //[10, 30]
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

void test_bindec_encoding_zero() {
    std::cout << "test_bindec_encoding_zero" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    BinDecConverter bindec_converter = BinDecConverter(ZERO);
    messages.push_back(std::make_pair("1234567890",             "12 34 56 78 90 F0 00 00 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("0987654321",             "09 87 65 43 21 F0 00 00 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("123456789012345",        "12 34 56 78 90 12 34 5F 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("567890987654321",        "56 78 90 98 76 54 32 1F 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("87638103692640182746",   "87 63 81 03 69 26 40 18 27 46 F0 00 00 00 00 00"));
    messages.push_back(std::make_pair("98275192837768916432",   "98 27 51 92 83 77 68 91 64 32 F0 00 00 00 00 00"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_encode = string_to_hexstring(bindec_converter.encode(it->first));
        if(it->second.compare(bindec_encode) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "Encode: " << bindec_encode << std::endl;
        }
    }
}

void test_bindec_decoding_zero() {
    std::cout << "test_bindec_decoding_zero" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    BinDecConverter bindec_converter = BinDecConverter(ZERO);
    messages.push_back(std::make_pair("1234567890",             "12 34 56 78 90 F0 00 00 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("0987654321",             "09 87 65 43 21 F0 00 00 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("123456789012345",        "12 34 56 78 90 12 34 5F 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("567890987654321",        "56 78 90 98 76 54 32 1F 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("87638103692640182746",   "87 63 81 03 69 26 40 18 27 46 F0 00 00 00 00 00"));
    messages.push_back(std::make_pair("98275192837768916432",   "98 27 51 92 83 77 68 91 64 32 F0 00 00 00 00 00"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_decode = bindec_converter.decode(string_from_hexstring(it->second));
        if(it->first.compare(bindec_decode) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "HexConv:" << string_to_hexstring(string_from_hexstring(it->second)) << std::endl;
            std::cout << "Encode: " << bindec_decode << std::endl;
        }
    }
}

void test_bindec_encoding_cycle_forward() {
    std::cout << "test_bindec_encoding_cycle_forward" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    BinDecConverter bindec_converter = BinDecConverter(CYCLE_FORWARD);
    messages.push_back(std::make_pair("1234567890",             "12 34 56 78 90 F1 23 45 67 89 01 23 45 67 89 01"));
    messages.push_back(std::make_pair("0987654321",             "09 87 65 43 21 F0 98 76 54 32 10 98 76 54 32 10"));
    messages.push_back(std::make_pair("123456789012345",        "12 34 56 78 90 12 34 5F 12 34 56 78 90 12 34 51"));
    messages.push_back(std::make_pair("567890987654321",        "56 78 90 98 76 54 32 1F 56 78 90 98 76 54 32 15"));
    messages.push_back(std::make_pair("87638103692640182746",   "87 63 81 03 69 26 40 18 27 46 F8 76 38 10 36 92"));
    messages.push_back(std::make_pair("98275192837768916432",   "98 27 51 92 83 77 68 91 64 32 F9 82 75 19 28 37"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_encode = string_to_hexstring(bindec_converter.encode(it->first));
        if(it->second.compare(bindec_encode) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "Encode: " << bindec_encode << std::endl;
        }
    }
}

void test_bindec_decoding_cycle_forward() {
    std::cout << "test_bindec_decoding_cycle_forward" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    BinDecConverter bindec_converter = BinDecConverter(CYCLE_FORWARD);
    messages.push_back(std::make_pair("1234567890",             "12 34 56 78 90 F1 23 45 67 89 01 23 45 67 89 01"));
    messages.push_back(std::make_pair("0987654321",             "09 87 65 43 21 F0 98 76 54 32 10 98 76 54 32 10"));
    messages.push_back(std::make_pair("123456789012345",        "12 34 56 78 90 12 34 5F 12 34 56 78 90 12 34 51"));
    messages.push_back(std::make_pair("567890987654321",        "56 78 90 98 76 54 32 1F 56 78 90 98 76 54 32 15"));
    messages.push_back(std::make_pair("87638103692640182746",   "87 63 81 03 69 26 40 18 27 46 F8 76 38 10 36 92"));
    messages.push_back(std::make_pair("98275192837768916432",   "98 27 51 92 83 77 68 91 64 32 F9 82 75 19 28 37"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_decode = bindec_converter.decode(string_from_hexstring(it->second));
        if(it->first.compare(bindec_decode) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "Encode: " << bindec_decode << std::endl;
        }
    }
}

void test_bindec_encoding_cycle_backward() {
    std::cout << "test_bindec_encoding_cycle_backward" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    BinDecConverter bindec_converter = BinDecConverter(CYCLE_BACKWARD);
    messages.push_back(std::make_pair("1234567890",             "12 34 56 78 90 F0 98 76 54 32 10 98 76 54 32 10"));
    messages.push_back(std::make_pair("0987654321",             "09 87 65 43 21 F1 23 45 67 89 01 23 45 67 89 01"));
    messages.push_back(std::make_pair("123456789012345",        "12 34 56 78 90 12 34 5F 54 32 10 98 76 54 32 15"));
    messages.push_back(std::make_pair("567890987654321",        "56 78 90 98 76 54 32 1F 12 34 56 78 90 98 76 51"));
    messages.push_back(std::make_pair("87638103692640182746",   "87 63 81 03 69 26 40 18 27 46 F6 47 28 10 46 29"));
    messages.push_back(std::make_pair("98275192837768916432",   "98 27 51 92 83 77 68 91 64 32 F2 34 61 98 67 73"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_encode = string_to_hexstring(bindec_converter.encode(it->first));
        if(it->second.compare(bindec_encode) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "Encode: " << bindec_encode << std::endl;
        }
    }
}

void test_bindec_decoding_cycle_backward() {
    std::cout << "test_bindec_decoding_cycle_backward" << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    BinDecConverter bindec_converter = BinDecConverter(CYCLE_BACKWARD);
    messages.push_back(std::make_pair("1234567890",             "12 34 56 78 90 F0 98 76 54 32 10 98 76 54 32 10"));
    messages.push_back(std::make_pair("0987654321",             "09 87 65 43 21 F1 23 45 67 89 01 23 45 67 89 01"));
    messages.push_back(std::make_pair("123456789012345",        "12 34 56 78 90 12 34 5F 54 32 10 98 76 54 32 15"));
    messages.push_back(std::make_pair("567890987654321",        "56 78 90 98 76 54 32 1F 12 34 56 78 90 98 76 51"));
    messages.push_back(std::make_pair("87638103692640182746",   "87 63 81 03 69 26 40 18 27 46 F6 47 28 10 46 29"));
    messages.push_back(std::make_pair("98275192837768916432",   "98 27 51 92 83 77 68 91 64 32 F2 34 61 98 67 73"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_decode = bindec_converter.decode(string_from_hexstring(it->second));
        if(it->first.compare(bindec_decode) == 0)
            std::cout << "Result:    Success!" << std::endl;
        else {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "Encode: " << bindec_decode << std::endl;
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
            std::cout << string_to_hexstring(encode_bindec) << std::endl;
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
    test_bindec_encoding_zero();
    test_bindec_decoding_zero();
    test_bindec_end_to_end_zero();

    test_bindec_encoding_cycle_forward();
    test_bindec_decoding_cycle_forward();
    test_bindec_end_to_end_cycle_forward();

    test_bindec_encoding_cycle_backward();
    test_bindec_decoding_cycle_backward();
    test_bindec_end_to_end_cycle_backward();

    test_bindec_end_to_end_random();
    
    //aes
    test_aes_coding();

    test_full_coding();
    test_dek_coding();
}
