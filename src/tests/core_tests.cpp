// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <iostream>
#include <vector>
#include <utility>

#include "utils.h"
#include "aes_crypter.h"


#define BINDEC_TESTS    50
#define AES_TESTS       50
#define AES_LONG_TESTS  50
#define FULL_TESTS      50


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

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#define B64_TESTS       50

TEST_CASE( "Testing HEX string operations", "[hex][string]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"", ""},
        {"A", "41"},
        {"AB", "41 42"},
        {"NOX", "4E 4F 58"},
    };
    for (const auto &p : cases) {
        SECTION( "testing encoding" ) {
            std::string encoded = string_to_hexstring(p.first);
            REQUIRE(p.second == encoded);
        }
    }
    for (const auto &p : cases) {
        SECTION( "testing decoding" ) {
            std::string decoded = string_from_hexstring(p.second);
            REQUIRE(p.first == decoded);
        }
    }
}

TEST_CASE( "Testing HEX string operations, without spaces", "[hex][string]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"", ""},
        {"A", "41"},
        {"AB", "4142"},
        {"NOX", "4e4f58"},
    };
    for (const auto &p : cases) {
        SECTION( "testing encoding" ) {
            std::string encoded = string_to_hexstring(p.first, HEX_LOWERCASE|HEX_NOSPACES);
            REQUIRE(p.second == encoded);
        }
    }
    for (const auto &p : cases) {
        SECTION( "testing decoding" ) {
            std::string decoded = string_from_hexstring(p.second, HEX_NOSPACES);
            REQUIRE(p.first == decoded);
        }
    }
}

TEST_CASE( "Testing HEX string error handling", "[hex][string][error]" ) {
    REQUIRE_THROWS( string_from_hexstring("4") );
    REQUIRE_THROWS( string_from_hexstring("4", HEX_NOSPACES) );
    REQUIRE_THROWS( string_from_hexstring("41 ") );
    REQUIRE_THROWS( string_from_hexstring("41 4") );
    REQUIRE_THROWS( string_from_hexstring("41_41") );
    REQUIRE_THROWS( string_from_hexstring("414", HEX_NOSPACES) );
    REQUIRE_THROWS( string_from_hexstring("4_") );
    REQUIRE_THROWS( string_from_hexstring("4g") );
}

TEST_CASE( "Testing BASE64 random strings", "[base64]" ) {
    std::vector<std::string> cases;
    for (int i = 0; i < B64_TESTS; ++i) {
        int rnd_len = rand() % 20;
        std::string gen_str = generate_random_string(rnd_len);
        REQUIRE( rnd_len == gen_str.size() );
        cases.push_back(gen_str);
    }

    for (const auto &p : cases) {
        SECTION( "testing coding" ) {
            std::string encode64 = encode_base64(p);
            std::string decode64 = decode_base64(encode64);
            REQUIRE( p == decode64 );
        }
    }
}

TEST_CASE( "Testing BASE64 coding", "[base64]" ) {
    std::vector<std::pair<std::string, std::string> > cases;
    cases.push_back(std::make_pair("",      ""));
    cases.push_back(std::make_pair(" ",     "IA=="));
    cases.push_back(std::make_pair("A",     "QQ=="));
    cases.push_back(std::make_pair("AB",    "QUI="));
    cases.push_back(std::make_pair("ABC",   "QUJD"));
    cases.push_back(std::make_pair("ABCD",  "QUJDRA=="));

    //for (int i = 0; i < 50; ++i) {
    //    int rnd_len = rand() % 50 + 1;
    //    std::string gen_str = generate_random_string(rnd_len);
    //    REQUIRE( rnd_len == gen_str.size() );
    //    cases.push_back(gen_str);
    //}
    
    for (const auto &p : cases) {
        SECTION( "testing encoding" ) {
            std::string encode64 = encode_base64(p.first);
            REQUIRE( p.second == encode64 );
        }
    }

    for(const auto &p : cases) {
        SECTION( "testing decoding" ) {
            std::string decode64 = decode_base64(p.second);
            REQUIRE( p.first == decode64 );
        }
    }

    for(const auto &p : cases) {
        SECTION( "testing coding" ) {
            std::string encode64 = encode_base64(p.first);
            std::string decode64 = decode_base64(encode64);
            REQUIRE( p.first == decode64 );
        }
    }
}

TEST_CASE( "Testing BCD encoder output size", "[bcd]" ) {
    REQUIRE( 4 == std::string(8, 0).substr(1, 4).size() );
    REQUIRE( 16 == bcd_encode("1234567890123456").size() );
    REQUIRE( 16 == bcd_encode("12345678901234567890").size() );
    REQUIRE( 16 == bcd_encode("1234567890123").size() );
    REQUIRE( 16 == bcd_encode("123").size() );
}

TEST_CASE( "Testing BCD coding", "[bcd]" ) {
    std::vector<std::pair<std::string, std::string> > cases;
    cases.push_back(std::make_pair("", ""));
    cases.push_back(std::make_pair("1",
                "1F 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11"));
    cases.push_back(std::make_pair("12",
                "12 F1 21 21 21 21 21 21 21 21 21 21 21 21 21 21"));
    cases.push_back(std::make_pair("87638103692640182746",
                "87 63 81 03 69 26 40 18 27 46 F0 00 00 00 00 00"));
    cases.push_back(std::make_pair("982751928377689164327",
                "98 27 51 92 83 77 68 91 64 32 7F 00 00 00 00 00"));
    cases.push_back(std::make_pair("1234567890123456123456789012345",
                "12 34 56 78 90 12 34 56 12 34 56 78 90 12 34 5F"));
    cases.push_back(std::make_pair("12345678901234561234567890123456",
                "12 34 56 78 90 12 34 56 12 34 56 78 90 12 34 56"));

    //add random cases

    for(const auto &p: cases) {
        SECTION( "testing encoding" ) { 
            std::string encode_bcd = bcd_encode(p.first);
            REQUIRE( p.second == encode_bcd);
        }
    }

    for(const auto &p: cases) {
        SECTION( "testing decoding" ) { 
            std::string decode_bcd = bcd_decode(p.second);
            REQUIRE( p.first == decode_bcd);
        }
    }
}

// vim:ts=4:sts=4:sw=4:et:
