#include <iostream>
#include <vector>
#include <utility>

#include "utils.h"
#include "aes_crypter.h"


#define B64_TESTS       50
#define BINDEC_TESTS    50
#define AES_TESTS       50
#define AES_LONG_TESTS  50
#define FULL_TESTS      50

void test_hex_string()
{
    std::cout << "test_hex_string..." << std::endl;
    std::vector<std::pair<std::string, std::string> > cases;
    cases.push_back(std::make_pair("", ""));
    cases.push_back(std::make_pair("A", "41"));
    cases.push_back(std::make_pair("AB", "41 42"));
    cases.push_back(std::make_pair("NOX", "4E 4F 58"));
    for (auto it = cases.begin(); it != cases.end(); ++it) {
        std::string encoded = string_to_hexstring(it->first);
        if (encoded != it->second) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->first << " Encoded: " << encoded << std::endl;
        }
    }
    for (auto it = cases.begin(); it != cases.end(); ++it) {
        std::string decoded = string_from_hexstring(it->second);
        if (decoded != it->first) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->first << " Decoded: " << decoded << std::endl;
        }
    }
}

void test_hex_string_no_spaces()
{
    std::cout << "test_hex_string_no_spaces..." << std::endl;
    std::vector<std::pair<std::string, std::string> > cases;
    cases.push_back(std::make_pair("", ""));
    cases.push_back(std::make_pair("A", "41"));
    cases.push_back(std::make_pair("AB", "4142"));
    cases.push_back(std::make_pair("NOX", "4e4f58"));
    for (auto it = cases.begin(); it != cases.end(); ++it) {
        std::string encoded = string_to_hexstring(it->first, HEX_LOWERCASE|HEX_NOSPACES);
        if (encoded != it->second) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->first << " Encoded: " << encoded << std::endl;
        }
    }
    for (auto it = cases.begin(); it != cases.end(); ++it) {
        std::string decoded = string_from_hexstring(it->second, HEX_NOSPACES);
        if (decoded != it->first) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->first << " Decoded: " << decoded << std::endl;
        }
    }
}

void test_base64_end_to_end() {
    std::cout << "test_base64_end_to_end..." << std::endl;
    std::vector<std::string> messages;
    for(int i = 0; i < B64_TESTS; i++)
        messages.push_back(generate_random_string(rand() % 20 + 10));
    for (int i = 0; i < B64_TESTS ; ++i) {
        std::string encode64 = encode_base64(messages[i]);
        std::string decode64 = decode_base64(encode64);
        if(messages[i].compare(decode64) != 0) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << messages[i] << " Decode: " << decode64 << std::endl;
        }
    }
}

void test_base64_encoding() {
    std::cout << "test_base64_encoding..." << std::endl;
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
        if(it->second.compare(encode64) != 0) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->first << " Encode: " << encode64 << std::endl;
        }
    }
}

void test_base64_decoding() {
    std::cout << "test_base64_decoding..." << std::endl;
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
    // as well as some more prosy samples
    //messages.push_back(std::make_pair("", ""));
    messages.push_back(std::make_pair("A", "QQ=="));
    messages.push_back(std::make_pair("AB", "QUI="));
    messages.push_back(std::make_pair("ABC", "QUJD"));
    messages.push_back(std::make_pair("ABCD", "QUJDRA=="));

    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string decode64 = decode_base64(it->second);
        if(it->first.compare(decode64) != 0) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input: " << it->second << " Decode: " << decode64 << std::endl;
        }
    }
}

void test_bindec_encoding() {
    std::cout << "test_bindec_encoding..." << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    messages.push_back(std::make_pair("1234567890123456",
                "12 34 56 78 90 12 34 56 F1 23 45 67 89 01 23 45"));
    messages.push_back(std::make_pair("12345678901234567",
                "12 34 56 78 90 12 34 56 7F 12 34 56 78 90 12 34"));

    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_encode = string_to_hexstring(bcd_encode(it->first));
        if(it->second.compare(bindec_encode) != 0) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "Encode: " << bindec_encode << std::endl;
        }
    }
}

void test_bindec_decoding() {
    std::cout << "test_bindec_decoding..." << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    messages.push_back(std::make_pair("",
                "F0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("9",
                "9F 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"));
    messages.push_back(std::make_pair("87638103692640182746",
                "87 63 81 03 69 26 40 18 27 46 F0 00 00 00 00 00"));
    messages.push_back(std::make_pair("982751928377689164327",
                "98 27 51 92 83 77 68 91 64 32 7F 00 00 00 00 00"));
    messages.push_back(std::make_pair("1234567890123456123456789012345",
                "12 34 56 78 90 12 34 56 12 34 56 78 90 12 34 5F"));
    messages.push_back(std::make_pair("12345678901234561234567890123456",
                "12 34 56 78 90 12 34 56 12 34 56 78 90 12 34 56"));

    for(it = messages.begin(); it != messages.end(); ++it) {
        std::string bindec_decode = bcd_decode(string_from_hexstring(it->second));
        if(it->first.compare(bindec_decode) != 0) {
            std::cout << "Result:    Fail!" << std::endl;
            std::cout << "Input:  " << it->first << std::endl; 
            std::cout << "Wait:   " << it->second << std::endl;
            std::cout << "HexConv:" << string_to_hexstring(
                    string_from_hexstring(it->second)) << std::endl;
            std::cout << "Encode: " << bindec_decode << std::endl;
        }
    }
}

void test_aes_end_to_end() {
    std::cout << "test_aes_end_to_end..." << std::endl;
    std::vector<std::string> messages;
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);
    for(int i = 0; i < AES_TESTS; i++)
        messages.push_back(generate_random_string(16 * (rand() % 5 + 1))); //[16, 32, 48, 64, 80]
    for(int i = 0; i < AES_TESTS; ++i) {
        try {   
            std::string encode_aes = aes_crypter.encrypt(messages[i]);
            std::string decode_aes = aes_crypter.decrypt(encode_aes);
            if (messages[i].compare(decode_aes) != 0) { 
                std::cout << "Result:    Fail!" << std::endl;
                std::cout << "Input: " << messages[i] 
                    << " Decode: " << decode_aes << std::endl;
            }
        } catch(AESBlockSizeException &exc) {
            std::cout << "Invalid block size: " << exc.what() << std::endl;
        }
    }
}

void test_aes_encoding() { 
    std::cout << "test_aes_encoding..." << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);
    messages.push_back(std::make_pair("1234567890123456",   "CF 21 48 02 A9 EA F1 F8 A1 68 91 6A 80 7F 60 54"));
    messages.push_back(std::make_pair("abcdefghijklmnop",   "CB 3B D7 7F 8B 72 28 4A 17 66 21 88 4E 3E 06 C9"));
    messages.push_back(std::make_pair("ABCDEFGHIJKLMNOP",   "08 7F B2 43 85 52 94 E2 00 2D B9 59 B4 D8 95 27"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        try { 
            std::string aes_encode = aes_crypter.encrypt(it->first);
            if(it->second.compare(string_to_hexstring(aes_encode)) != 0) { 
                std::cout << "Result:    Fail!" << std::endl;
                std::cout << "Input:  " << it->first << std::endl; 
                std::cout << "Wait:   " << it->second << std::endl;
                std::cout << "Encode: " << aes_encode << std::endl;
            }
        } catch(AESBlockSizeException &exc) {
            std::cout << "Invalid block size: " << exc.what() << std::endl;
        }
    }
}

void test_aes_decoding() { 
    std::cout << "test_aes_decoding..." << std::endl;
    std::vector<std::pair<std::string, std::string> > messages;
    std::vector<std::pair<std::string, std::string> >::iterator it;
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);
    messages.push_back(std::make_pair("1234567890123456",   "CF 21 48 02 A9 EA F1 F8 A1 68 91 6A 80 7F 60 54"));
    messages.push_back(std::make_pair("abcdefghijklmnop",   "CB 3B D7 7F 8B 72 28 4A 17 66 21 88 4E 3E 06 C9"));
    messages.push_back(std::make_pair("ABCDEFGHIJKLMNOP",   "08 7F B2 43 85 52 94 E2 00 2D B9 59 B4 D8 95 27"));
    
    for(it = messages.begin(); it != messages.end(); ++it) {
        try { 
            std::string aes_decode = aes_crypter.decrypt(string_from_hexstring(it->second));
            if(it->first.compare(aes_decode) != 0) { 
                std::cout << "Result:    Fail!" << std::endl;
                std::cout << "Input:  " << it->first << std::endl; 
                std::cout << "Wait:   " << it->second << std::endl;
                std::cout << "Encode: " << aes_decode << std::endl;
            }
        } catch(AESBlockSizeException &exc) {
            std::cout << "Invalid block size: " << exc.what() << std::endl;
        }
    }
}
            
void test_coding_end_to_end() {
    std::cout << "test_coding_end_to_end..." << std::endl;
    std::vector<std::string> messages;
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);
    for(int i = 0; i < FULL_TESTS; i++)
        messages.push_back(generate_random_number(rand() % 5 + 16));

    for(int i = 0; i < FULL_TESTS; i++) {
        try {
            std::string encode_bindec = bcd_encode(messages[i]);
            std::string encode_aes = aes_crypter.encrypt(encode_bindec);
            std::string encode_b64 = encode_base64(encode_aes);
            std::string decode_b64 = decode_base64(encode_b64);
            std::string decode_aes = aes_crypter.decrypt(decode_b64);
            std::string decode_bindec = bcd_decode(decode_aes); 
            if (messages[i].compare(decode_bindec) != 0) { 
                std::cout << "Result:    Fail!" << std::endl;
                std::cout << "Input: " << messages[i] << " Decode: " << decode_aes << "." << std::endl;
            }
        } catch(AESBlockSizeException &exc) {
            std::cout << "Invalid block size: " << exc.what() << std::endl;
        }
    }
}

    
//TODO: random data generation
//TODO: unittesting
//TODO: add predicted tests
int main() {
    // hex
    test_hex_string();
    test_hex_string_no_spaces();

    //base64
    test_base64_encoding();
    test_base64_decoding();
    test_base64_end_to_end();

    //bindec
    test_bindec_encoding();
    test_bindec_decoding();
    
    //aes
    test_aes_end_to_end();
    test_aes_encoding();
    test_aes_decoding();

    test_coding_end_to_end();
}
