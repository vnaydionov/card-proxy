// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <iostream>
#include <utility>
#include <vector>

#include "aes_crypter.h"
#include "catch.hpp"
#include "utils.h"

#define B64_TESTS       50
#define BCD_TESTS       50
#define AES_TESTS       50
#define FULL_TESTS      50

TEST_CASE( "Testing format string escaping", "[str]" ) {
    CHECK("abc,def" == fmt_string_escape("abc,def"));
    CHECK("abc%%def" == fmt_string_escape("abc%def"));
    CHECK("abc%%%%def" == fmt_string_escape("abc%%def"));
}

TEST_CASE( "Testing HEX string operations", "[hex][string]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"", ""},
        {"A", "41"},
        {"AB", "41 42"},
        {"NOX", "4E 4F 58"},
    };

    SECTION( "testing encoding" ) {
        for (const auto &p : cases) {
            std::string encoded = string_to_hexstring(p.first);
            CHECK(p.second == encoded);
        }
    }

    SECTION( "testing decoding" ) {
        for (const auto &p : cases) {
            std::string decoded = string_from_hexstring(p.second);
            CHECK(p.first == decoded);
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

    SECTION( "testing encoding" ) {
        for (const auto &p : cases) {
            std::string encoded = string_to_hexstring(p.first, HEX_LOWERCASE|HEX_NOSPACES);
            CHECK(p.second == encoded);
        }
    }

    SECTION( "testing decoding" ) {
        for (const auto &p : cases) {
            std::string decoded = string_from_hexstring(p.second, HEX_NOSPACES);
            CHECK(p.first == decoded);
        }
    }
}

TEST_CASE( "Testing HEX string error handling", "[hex][string]" ) {
    CHECK_THROWS( string_from_hexstring("4") );
    CHECK_THROWS( string_from_hexstring("4", HEX_NOSPACES) );
    CHECK_THROWS( string_from_hexstring("41 ") );
    CHECK_THROWS( string_from_hexstring("41 4") );
    CHECK_THROWS( string_from_hexstring("41_41") );
    CHECK_THROWS( string_from_hexstring("414", HEX_NOSPACES) );
    CHECK_THROWS( string_from_hexstring("4_") );
    CHECK_THROWS( string_from_hexstring("4g") );
}

TEST_CASE( "Testing BASE64 coding", "[base64]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"",      ""},
        {" ",     "IA=="},
        {"A",     "QQ=="},
        {"AB",    "QUI="},
        {"ABC",   "QUJD"},
        {"ABCD",  "QUJDRA=="},
    };
    
    SECTION( "testing encoding" ) {
        for (const auto &p : cases) {
            std::string encode64 = encode_base64(p.first);
            CHECK( p.second == encode64 );
        }
    }

    SECTION( "testing decoding" ) {
        for(const auto &p : cases) {
            std::string decode64 = decode_base64(p.second);
            CHECK( p.first == decode64 );
        }
    }

    SECTION( "testing encode/decode" ) {
        for(const auto &p : cases) {
            std::string encode64 = encode_base64(p.first);
            std::string decode64 = decode_base64(encode64);
            CHECK( p.first == decode64 );
        }
    }
}

TEST_CASE( "Testing BASE64 random strings", "[base64]" ) {
    std::vector<std::string> cases;
    for (int i = 0; i < B64_TESTS; ++i) {
        int rnd_len = rand() % 20;
        std::string gen_str = generate_random_string(rnd_len);
        REQUIRE( rnd_len == gen_str.size() );
        cases.push_back(gen_str);
    }

    SECTION( "testing coding" ) {
        for (const auto &p : cases) {
            std::string encode64 = encode_base64(p);
            std::string decode64 = decode_base64(encode64);
            CHECK( p == decode64 );
        }
    }
}


TEST_CASE( "Testing BCD encoder output size", "[bcd]" ) {
    CHECK( 4 == std::string(8, 0).substr(1, 4).size() );
    CHECK( 16 == bcd_encode("1234567890123456").size() );
    CHECK( 16 == bcd_encode("12345678901234567890").size() );
    CHECK( 16 == bcd_encode("1234567890123").size() );
    CHECK( 16 == bcd_encode("123").size() );
}

TEST_CASE( "Testing BCD coding", "[bcd]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"",   ""},
        {"1",  "1F"},
        {"12", "12 F"},
        {"87638103692640182746",
            "87 63 81 03 69 26 40 18 27 46 F"},
        {"982751928377689164327",
            "98 27 51 92 83 77 68 91 64 32 7F "},
        {"1234567890123456123456789012345",
            "12 34 56 78 90 12 34 56 12 34 56 78 90 12 34 5F"},
    };

    SECTION( "testing encoding" ) { 
        for(const auto &p : cases) {
            std::string encode_bcd = bcd_encode(p.first);
            CAPTURE(p.first);
            if (!p.first.size()) {
                CHECK( encode_bcd.size() == 0 );
            }
            else {
                CHECK( encode_bcd.size() == 16 );
                CHECK( p.second ==
                        string_to_hexstring(encode_bcd).substr(0, p.second.size()) );
            }
        }
    }

    SECTION( "testing decoding" ) { 
        for(const auto &p : cases) {
            std::string code;
            if (!p.second.empty()) {
                code = string_from_hexstring(
                    p.second + std::string(
                        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
                    ).substr(p.second.size()));
            }
            std::string decode_bcd = bcd_decode(code);
            CHECK( p.first == decode_bcd );
        }
    }
}

TEST_CASE( "Testing BCD random_coding", "[bcd]" ) {
    std::vector<std::string> cases;
    for(int i = 0; i < BCD_TESTS; ++i) {
        int rnd_len = rand() % 20 + 1;
        std::string gen_str = generate_random_number(rnd_len);
        REQUIRE( rnd_len == gen_str.size() );
        cases.push_back(gen_str);
    }

    SECTION( "testing encode/decode" ) {
        for(const auto &p : cases) {
            std::string encode_bcd = bcd_encode(p);
            std::string decode_bcd = bcd_decode(encode_bcd);
            CHECK( p == decode_bcd);
        }
    }
}

TEST_CASE( "Testing BCD errors", "[bcd]" ) {
    CHECK_NOTHROW( bcd_encode(generate_random_number(30)) );
    CHECK_NOTHROW( bcd_encode(generate_random_number(31)) );
    CHECK_THROWS( bcd_encode(generate_random_number(32)) );
    CHECK_THROWS( bcd_encode(generate_random_number(33)) );
}

TEST_CASE( "Testing AES coding", "[aes]") {
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);
    std::vector<std::pair<std::string, std::string>> cases{
        {"1234567890123456",
            "CF 21 48 02 A9 EA F1 F8 A1 68 91 6A 80 7F 60 54"},
        {"abcdefghijklmnop",
            "CB 3B D7 7F 8B 72 28 4A 17 66 21 88 4E 3E 06 C9"},
        {"ABCDEFGHIJKLMNOP",
            "08 7F B2 43 85 52 94 E2 00 2D B9 59 B4 D8 95 27"},
    };
    
    SECTION( "testing encrypt" ) {
        for(const auto &p : cases) { 
            std::string aes_encode = aes_crypter.encrypt(p.first);
            CHECK(p.second == string_to_hexstring(aes_encode));
        }
    }

    SECTION( "testing decrypt" ) {
        for(const auto &p : cases) { 
            std::string code = string_from_hexstring(p.second);
            std::string aes_decode = aes_crypter.decrypt(code);
            CAPTURE(p.second);
            CAPTURE(string_to_hexstring(code));
            CHECK(p.first == aes_decode);
        }
    }
}

TEST_CASE( "Testing AES random coding", "[aes]") {
    std::vector<std::string> cases;
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);
    for(int i = 0; i < AES_TESTS; i++) {
        cases.push_back(generate_random_string(16 * (rand() % 5 + 1)));
    }

    SECTION( "testing encode/decode" ) { 
        for(const auto &p : cases) {
            std::string encode_aes = aes_crypter.encrypt(p);
            std::string decode_aes = aes_crypter.decrypt(encode_aes);
            CHECK(p == decode_aes);
        }
    }
}

TEST_CASE( "Testing AES errors", "[aes]") {
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);

    CHECK_THROWS( aes_crypter.encrypt("") );
    CHECK_THROWS( aes_crypter.encrypt("1234") );
    CHECK_THROWS( aes_crypter.encrypt("ABCD") );
    CHECK_THROWS( aes_crypter.encrypt("12345678") );
    CHECK_THROWS( aes_crypter.encrypt("ABCDEFGH") );
    CHECK_THROWS( aes_crypter.encrypt("123456789012") );
    CHECK_THROWS( aes_crypter.encrypt("ABCDEFGHIJKL") );
}

TEST_CASE( "Testing full coding", "[full][base64][aes][bcd]") {
    std::vector<std::string> cases;
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key);
    for(int i = 0; i < FULL_TESTS; i++) {
        cases.push_back(generate_random_number(rand() % 5 + 16));
    }

    SECTION( "testing encode/decode" ) {
        for(const auto &p : cases) {
            std::string encode_bindec = bcd_encode(p);
            std::string encode_aes =    aes_crypter.encrypt(encode_bindec);
            std::string encode_b64 =    encode_base64(encode_aes);
            std::string decode_b64 =    decode_base64(encode_b64);
            std::string decode_aes =    aes_crypter.decrypt(decode_b64);
            std::string decode_bindec = bcd_decode(decode_aes); 
            CHECK(p == decode_bindec);
        }
    }
}

TEST_CASE( "Testing PAN masking", "[full][mask]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"1234567890123456", "123456******3456"},
        {"12345678901234", "123456****1234"},
        {"123456789012345678", "123456********5678"},
    };
    SECTION( "testing masks" ) {
        for (const auto &p : cases) {
            CHECK(p.second == mask_pan(p.first));
        }
    }
    SECTION( "trying to mask PAN of invalid length" ) {
        CHECK_THROWS( mask_pan("") );
        CHECK_THROWS( mask_pan("123456789012") );
        CHECK_THROWS( mask_pan("12345678901234567890123") );
    }
}

TEST_CASE( "Testing PAN normalization", "[full][normalize]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"  1234 5678 9012 3456\t\n", "1234567890123456"},
        {"1 2 3 4  5678901\n\n2 \t 34", "12345678901234"},
        {"12345678901\n\n2 \t 34567890  ", "12345678901234567890"},
    };
    SECTION( "testing normalize" ) {
        for (const auto &p : cases) {
            CHECK(p.second == normalize_pan(p.first));
        }
    }
    SECTION( "trying to normalize PAN with invalid characters" ) {
        CHECK_THROWS( normalize_pan("1234567890123456*") );
        CHECK_THROWS( normalize_pan("*1234 5678 9012 3456") );
        CHECK_THROWS( normalize_pan("1234 5678 -012 3456") );
    }
}


// vim:ts=4:sts=4:sw=4:et:
