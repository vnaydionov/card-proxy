// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <iostream>
#include <utility>
#include <vector>
#include <util/string_utils.h>

#include "catch.hpp"

#include "aes_crypter.h"
#include "utils.h"
#include "app_class.h"
#include "json_object.h"

#include "card_crypter.h"

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
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string encoded = string_to_hexstring(p.first);
            CHECK(p.second == encoded);
        }
    }

    SECTION( "testing decoding" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string encoded = string_to_hexstring(p.first, HEX_LOWERCASE|HEX_NOSPACES);
            CHECK(p.second == encoded);
        }
    }

    SECTION( "testing decoding" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string encode64 = encode_base64(p.first);
            CHECK( p.second == encode64 );
        }
    }

    SECTION( "testing decoding" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string decode64 = decode_base64(p.second);
            CHECK( p.first == decode64 );
        }
    }

    SECTION( "testing encode/decode" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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
        REQUIRE( rnd_len == (int)gen_str.size() );
        cases.push_back(gen_str);
    }

    SECTION( "testing coding" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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
        REQUIRE( rnd_len == (int)gen_str.size() );
        cases.push_back(gen_str);
    }

    SECTION( "testing encode/decode" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string encode_bcd = bcd_encode(p);
            std::string decode_bcd = bcd_decode(encode_bcd);
            CHECK( p == decode_bcd);
        }
    }
}

TEST_CASE( "Testing BCD errors", "[bcd]" ) {
    CHECK_NOTHROW( bcd_encode(generate_random_number(30)) );
    CHECK_NOTHROW( bcd_encode(generate_random_number(31)) );
    CHECK_NOTHROW( bcd_encode(generate_random_number(32)) );
    CHECK_THROWS( bcd_encode(generate_random_number(33)) );
}

TEST_CASE( "Testing PKCS7 encoder output size", "[pkcs7]" ) {
    CHECK( 4 == std::string(8, 0).substr(1, 4).size() );
    CHECK( 32 == pkcs7_encode("1234567890123456").size() );
    CHECK( 32 == pkcs7_encode("12345678901234567890").size() );
    CHECK( 16 == pkcs7_encode("1234567890123").size() );
    CHECK( 16 == pkcs7_encode("123").size() );
    CHECK( 16 == pkcs7_encode("").size() );
}

TEST_CASE( "Testing PKCS7 coding", "[pkcs7]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"",   "\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10"},
        {"1",  "1\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f"},
        {"12", "12\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e"},
        {"87638103692640182746",
            "87638103692640182746\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"},
        {"98275192837768916432927652854",
            "98275192837768916432927652854\x03\x03\x03"},
        {"abcdefghijklmnopabcdefghijklmno",
            "abcdefghijklmnopabcdefghijklmno\x01"},
    };

    SECTION( "testing encoding" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string encode_pkcs7 = pkcs7_encode(p.first);
            CHECK( p.second == encode_pkcs7 );
        }
    }

    SECTION( "testing decoding" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string decode_pkcs7 = pkcs7_decode(p.second);
            CHECK( p.first == decode_pkcs7 );
        }
    }
}

TEST_CASE( "Testing PKCS7 errors", "[pkcs7]" ) {
    CHECK_NOTHROW( pkcs7_encode(generate_random_string(30)) );
    CHECK_NOTHROW( pkcs7_encode(generate_random_string(31)) );
    CHECK_NOTHROW( pkcs7_encode(generate_random_string(32)) );
    CHECK_NOTHROW( pkcs7_encode(generate_random_string(33)) );
    CHECK_THROWS( pkcs7_decode(std::string("")) );
    CHECK_THROWS( pkcs7_decode(std::string("xxx")) );
    CHECK_THROWS( pkcs7_decode(std::string(16, 0)) );
    CHECK_THROWS( pkcs7_decode(std::string(16, 0x11)) );
    CHECK_THROWS( pkcs7_decode(std::string(15, 0) + '\x02') );
}

TEST_CASE( "Testing AES coding", "[aes]") {
    std::string key = "12345678901234567890123456789012";
    std::vector<std::pair<std::string, std::string>> cases{
        {"1234567890123456",
            "CF 21 48 02 A9 EA F1 F8 A1 68 91 6A 80 7F 60 54"},
        {"abcdefghijklmnop",
            "CB 3B D7 7F 8B 72 28 4A 17 66 21 88 4E 3E 06 C9"},
        {"ABCDEFGHIJKLMNOP",
            "08 7F B2 43 85 52 94 E2 00 2D B9 59 B4 D8 95 27"},
        {"ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP",
            "08 7F B2 43 85 52 94 E2 00 2D B9 59 B4 D8 95 27 "
            "08 7F B2 43 85 52 94 E2 00 2D B9 59 B4 D8 95 27"},
    };
    std::string cbc_case =
            "08 7F B2 43 85 52 94 E2 00 2D B9 59 B4 D8 95 27 "
            "B3 0A 93 0E C3 7E B0 7A 79 26 F7 82 5C D8 80 9F";
    /*
    openssl enc -aes-256-cbc -in plain.txt -out encrypted.bin -nosalt \
      -K 3132333435363738393031323334353637383930313233343536373839303132 \
      -iv 00000000000000000000000000000000
    */

    SECTION( "testing encrypt" ) {
        AESCrypter aes_crypter(key, AES_CRYPTER_ECB);
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string aes_encode = aes_crypter.encrypt(p.first);
            CHECK(p.second == string_to_hexstring(aes_encode));
        }
    }

    SECTION( "testing decrypt" ) {
        AESCrypter aes_crypter(key, AES_CRYPTER_ECB);
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string code = string_from_hexstring(p.second);
            std::string aes_decode = aes_crypter.decrypt(code);
            CHECK(p.first == aes_decode);
        }
    }

    SECTION( "testing encrypt with CBC" ) {
        AESCrypter aes_crypter(key, AES_CRYPTER_CBC);
        auto i = cases.begin();
        for (; (i + 1) != cases.end(); ++i) {
            const auto &p = *i;
            std::string aes_encode = aes_crypter.encrypt(p.first);
            CHECK(p.second == string_to_hexstring(aes_encode));
        }
        const auto &p = *i;
        std::string aes_encode = aes_crypter.encrypt(p.first);
        CAPTURE(string_to_hexstring(aes_encode));
        CHECK(cbc_case == string_to_hexstring(aes_encode));
    }

    SECTION( "testing decrypt with CBC" ) {
        AESCrypter aes_crypter(key, AES_CRYPTER_CBC);
        auto i = cases.begin();
        for (; (i + 1) != cases.end(); ++i) {
            const auto &p = *i;
            std::string code = string_from_hexstring(p.second);
            std::string aes_decode = aes_crypter.decrypt(code);
            CHECK(p.first == aes_decode);
        }
        const auto &p = *i;
        std::string code = string_from_hexstring(cbc_case);
        std::string aes_decode = aes_crypter.decrypt(code);
        CHECK(p.first == aes_decode);
    }
}

TEST_CASE( "Testing AES random coding", "[aes]") {
    std::vector<std::string> cases;
    std::string key = "12345678901234567890123456789012";
    for(int i = 0; i < AES_TESTS; i++) {
        cases.push_back(generate_random_string(16 * (rand() % 5 + 1)));
    }

    SECTION( "testing encode/decode" ) {
        AESCrypter aes_crypter(key, AES_CRYPTER_ECB);
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string encode_aes = aes_crypter.encrypt(p);
            std::string decode_aes = aes_crypter.decrypt(encode_aes);
            CHECK(p == decode_aes);
        }
    }

    SECTION( "testing encode/decode with CBC" ) {
        AESCrypter aes_crypter(key, AES_CRYPTER_CBC);
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
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

TEST_CASE( "Testing full coding with PKCS7 and CBC mode", "[full][base64][aes][pkcs7]") {
    std::vector<std::string> cases;
    std::string key = "12345678901234567890123456789012";
    AESCrypter aes_crypter(key, AES_CRYPTER_CBC);
    for(int i = 0; i < FULL_TESTS; i++) {
        cases.push_back(generate_random_string(rand() % 48));
    }

    SECTION( "testing encode/decode" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            std::string encode_pkcs7 =  pkcs7_encode(p);
            std::string encode_aes =    aes_crypter.encrypt(encode_pkcs7);
            std::string encode_b64 =    encode_base64(encode_aes);
            std::string decode_b64 =    decode_base64(encode_b64);
            std::string decode_aes =    aes_crypter.decrypt(decode_b64);
            std::string decode_pkcs7 =  pkcs7_decode(decode_aes);
            CHECK( p == decode_pkcs7 );
        }
    }
}

TEST_CASE( "Testing PAN masking", "[full][mask]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"1234567890123456", "123456****3456"},
        {"123456789012", "12****9012"},
        {"1234567890123456789", "123456****6789"},
    };
    SECTION( "testing masks" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            CHECK(p.second == mask_pan(p.first));
        }
    }
    SECTION( "trying to mask PAN of invalid length" ) {
        CHECK_THROWS( mask_pan("") );
        CHECK_THROWS( mask_pan("12345678901") );
        CHECK_THROWS( mask_pan("12345678901234567890") );
    }
}

TEST_CASE( "Testing PAN normalization", "[full][normalize]" ) {
    std::vector<std::pair<std::string, std::string>> cases{
        {"  1234 5678 9012 3456\t\n", "1234567890123456"},
        {"1 2 3 4  5678901\n\n2 \t 34", "12345678901234"},
        {"12345678901\n\n2 \t 3456789  ", "1234567890123456789"},
    };
    SECTION( "testing normalize" ) {
        for (auto i = cases.begin(); i != cases.end(); ++i) {
            const auto &p = *i;
            CHECK(p.second == normalize_pan(p.first));
        }
    }
    SECTION( "trying to normalize PAN with invalid characters" ) {
        CHECK_THROWS( normalize_pan("1234567890123456*") );
        CHECK_THROWS( normalize_pan("*1234 5678 9012 3456") );
        CHECK_THROWS( normalize_pan("1234 5678 -012 3456") );
    }
}

TEST_CASE( "Testing PAN and Key filtering", "[full][filter_log]" ) {
    SECTION( "testing PAN filtering" ) {
        CHECK("045566XXXX3344" == filter_log_msg("0455667711223344"));
        CHECK("145566XXXX3344" == filter_log_msg("1455667711223344"));
        CHECK("245566XXXX3344" == filter_log_msg("2455667711223344"));
        CHECK("345566XXXX3344" == filter_log_msg("3455667711223344"));
        CHECK("445566XXXX3344" == filter_log_msg("4455667711223344"));
        CHECK("556677XXXX3344" == filter_log_msg("5566778811223344"));
        CHECK("656677XXXX3344" == filter_log_msg("6566778811223344"));
        CHECK("745566XXXX3344" == filter_log_msg("7455667711223344"));
        CHECK("845566XXXX3344" == filter_log_msg("8455667711223344"));
        CHECK("945566XXXX3344" == filter_log_msg("9455667711223344"));
        CHECK("556677XXXX3344" == filter_log_msg("55667788111223344"));
        CHECK("556677XXXX3344" == filter_log_msg("556677881111223344"));
        CHECK("556677XXXX3344" == filter_log_msg("5566778811111223344"));
        CHECK("55667788111111223344" == filter_log_msg("55667788111111223344"));
        CHECK("556677XXXX3344 " == filter_log_msg("5566778811223344 "));
        CHECK(" 556677XXXX3344" == filter_log_msg(" 5566778811223344"));
        CHECK("556677XXXX3344,445566XXXX3344"
                == filter_log_msg("5566778811223344,4455667711223344"));
    }
    SECTION( "testing Short PAN filtering" ) {
        CHECK("45561111222" == filter_log_msg("45561111222"));
        CHECK("05XXXX2222" == filter_log_msg("055611112222"));
        CHECK("15XXXX2222" == filter_log_msg("155611112222"));
        CHECK("25XXXX2222" == filter_log_msg("255611112222"));
        CHECK("35XXXX2222" == filter_log_msg("355611112222"));
        CHECK("45XXXX2222" == filter_log_msg("455611112222"));
        CHECK("55XXXX2222" == filter_log_msg("555611112222"));
        CHECK("65XXXX2222" == filter_log_msg("655611112222"));
        CHECK("75XXXX2222" == filter_log_msg("755611112222"));
        CHECK("85XXXX2222" == filter_log_msg("855611112222"));
        CHECK("95XXXX2222" == filter_log_msg("955611112222"));
        CHECK("45XXXX2223" == filter_log_msg("4556111122223"));
        CHECK("45XXXX2233" == filter_log_msg("45561111222233"));
        CHECK("45XXXX2333" == filter_log_msg("455611112222333"));
    }
    SECTION( "testing PAN filtering with delimiters" ) {
        CHECK("5566 77XXXX3344" == filter_log_msg("5566 7788 1122 3344"));
        CHECK("6566 77XXXX3344" == filter_log_msg("6566 7788 1122 13344"));
        CHECK("6566 77XXXX3344" == filter_log_msg("6566 7788 1122 223344"));
        CHECK("6566 77XXXX3344" == filter_log_msg("6566 7788 1122 3333344"));
        CHECK("5566\t77XXXX3344" == filter_log_msg("5566\t7788\t1122\t3344"));
        CHECK("5566-77XXXX3344" == filter_log_msg("5566-7788-1122-3344"));
    }
    SECTION( "testing short PAN filtering with delimiters" ) {
        CHECK("45-56111-1222" == filter_log_msg("45-56111-1222"));
        CHECK("05-XXXX2222" == filter_log_msg("05-561111-2222"));
        CHECK("15-XXXX22-22" == filter_log_msg("15-56-11-11-22-22"));
        CHECK("35\tXXXX2222" == filter_log_msg("35\t56\t1111\t2222"));
        CHECK("45 XXXX22 22" == filter_log_msg("45 56 11 11 22 22"));
        CHECK("65 XXXX2222" == filter_log_msg("65 56-11-11 2222"));
        CHECK("45-XXXX2223" == filter_log_msg("45-561-1112-2223"));
        CHECK("45 XXXX22 33" == filter_log_msg("45 56 11 11 22 22 33"));
        CHECK("45\tXXXX2333" == filter_log_msg("45\t561111222\t2333"));
        CHECK("4    5-----XXXX2&2!!2###2"
                == filter_log_msg("4    5-----5?????6 11 ?11 2&2!!2###2"));
        CHECK("4 5 XXXX2-3 3\t3" == filter_log_msg("4 5 5 6-1 1 1 1-2 2 2 2-3 3\t3"));
        CHECK("[u'0', u'5', u'XXXX2', u'2', u'2', u'2']"
                == filter_log_msg("[u'0', u'5', u'5', u'6', u'1', u'1', u'1', u'1', u'2', u'2', u'2', u'2']"));
    }
    SECTION( "testing Token filtering" ) {
        CHECK("65e84be33532fb784c48129675f9eff" == filter_log_msg(
                    "65e84be33532fb784c48129675f9eff"));
        CHECK("65e84be33532fb784c48129675f9effab" == filter_log_msg(
                    "65e84be33532fb784c48129675f9effab"));
        CHECK("65e8xxxx37c5" == filter_log_msg(
                    "65e84be368c0ea744b2cf58ee02337c5"));
        CHECK("65E8xxxx37C5" == filter_log_msg(
                    "65E84BE368C0EA744B2CF58EE02337C5"));
        CHECK(",65E8xxxx37C5" == filter_log_msg(
                    ",65E84BE368C0EA744B2CF58EE02337C5"));
        CHECK("65E8xxxx37C5," == filter_log_msg(
                    "65E84BE368C0EA744B2CF58EE02337C5,"));
        CHECK("65E8xxxx37C5,8588xxxx81cd" == filter_log_msg(
                    "65E84BE368C0EA744B2CF58EE02337C5,8588310792bdcd0c8f334867c54581cd"));
    }
    SECTION( "testing Key filtering" ) {
        CHECK("65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c50" == filter_log_msg(
                    "65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c50"));
        CHECK("65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c" == filter_log_msg(
                    "65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c"));
        CHECK("65e84be3....e02337c5" == filter_log_msg(
                    "65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c5"));
        CHECK("65E84BE3....E02337C5" == filter_log_msg(
                    "65E84BE33532FB784C48129675F9EFF3A682B27168C0EA744B2CF58EE02337C5"));
        CHECK(",65E84BE3....E02337C5" == filter_log_msg(
                    ",65E84BE33532FB784C48129675F9EFF3A682B27168C0EA744B2CF58EE02337C5"));
        CHECK("65E84BE3....E02337C5," == filter_log_msg(
                    "65E84BE33532FB784C48129675F9EFF3A682B27168C0EA744B2CF58EE02337C5,"));
        CHECK("65E84BE3....E02337C5,8588310a....c54581cd" == filter_log_msg(
                    "65E84BE33532FB784C48129675F9EFF3A682B27168C0EA744B2CF58EE02337C5,8588310a98676af6e22563c1559e1ae20f85950792bdcd0c8f334867c54581cd"));
    }
    SECTION( "HMAC Key filtering" ) {
        CHECK("VRms????Y1s=" == filter_log_msg("VRms1Uky1e1Yc58wgIuLeEcnowRfzElBV84PILmHY1s="));
        CHECK("azazaVRms1Uky1e1Yc58wgIuLeEcnowRfzElBV84PILmHY1s="
                == filter_log_msg("azazaVRms1Uky1e1Yc58wgIuLeEcnowRfzElBV84PILmHY1s="));
        CHECK("VRms????Y1s=,VRms????Y1s="
                == filter_log_msg("VRms1Uky1e1Yc58wgIuLeEcnowRfzElBV84PILmHY1s=,VRms1Uky1e1Yc58wgIuLeEcnowRfzElBV84PILmHY1s="));
        CHECK("\"VRms????Y1s=YYY\""
                == filter_log_msg("\"VRms1Uky1e1Yc58wgIuLeEcnowRfzElBV84PILmHY1s=YYY\""));
    }
    SECTION( "CVN filtering" ) {
        CHECK( "'cvn':\"XXX\",  'cvv2': '2345', \"CVV\":\"XXX\", 'cVc2': \"XXX\"" ==
                filter_log_msg("'cvn':'123',  'cvv2': '2345', \"CVV\":'324', 'cVc2': '345'") );
        CHECK( "cvn=XXX&cvv2=2345&CVV=XXX&cVc2=XXX" ==
                filter_log_msg("cvn=123&cvv2=2345&CVV=324&cVc2=345") );
    }
}

TEST_CASE( "Testing the escaping of non-printables", "[full][escape_nl]" ) {
    SECTION( "testing escaping of non-printables" ) {
        CHECK("" == escape_nl(""));
        CHECK("!abc DEF~" == escape_nl("!abc DEF~"));
        CHECK("#001#011#037. .#177#200#377" == escape_nl("\x01\x09\x1F.\x20.\x7F\x80\xFF"));
    }
}

TEST_CASE( "Some tests for JSON", "[full][json]" ) {
    SECTION( "check ownership passing" ) {
        JsonObject o = JsonObject::new_object();
        CHECK( o.owns() );
        JsonObject p = o;
        CHECK( p.owns() );
        CHECK( !o.owns() );
        JsonObject q(p.get(), false);
        CHECK( !q.owns() );
        q = p;
        CHECK( q.owns() );
        CHECK( !p.owns() );
    }
    SECTION( "parsing JSON" ) {
        JsonObject o = JsonObject::parse(
                "{\"hello\": \"1\", \"world\": 2, \"xxx\": false, \"yyy\": 1.5}");
        CHECK_THROWS( o.get_str_field("hell") );
        CHECK_THROWS( o.get_str_field("world") );
        CHECK("1" == o.get_str_field("hello"));
        CHECK("s:1" == o.get_typed_field("hello"));
        CHECK("i:2" == o.get_typed_field("world"));
        CHECK("b:false" == o.get_typed_field("xxx"));
        CHECK("f:1.5" == o.get_typed_field("yyy"));
    }
    SECTION( "crafting JSON" ) {
        CHECK_THROWS( JsonObject(NULL, true) );
        JsonObject o = JsonObject::new_object();
        o.add_str_field("aaa", "qwerty");
        o.add_typed_field("bbb", "s:asdfgh");
        o.add_typed_field("ccc", "i:-100500");
        o.add_typed_field("ddd", "b:true");
        o.add_typed_field("eee", "f:3.14");
        CHECK("{ \"aaa\": \"qwerty\", \"bbb\": \"asdfgh\", \"ccc\": -100500, "
                "\"ddd\": true, \"eee\": 3.140000 }" == o.serialize());
        o.delete_field("eee");
        o.delete_field("bbb");
        CHECK( o.has_field("aaa") );
        CHECK( !o.has_field("bbb") );
        CHECK( o.has_field("ccc") );
        CHECK( o.has_field("ddd") );
        CHECK( !o.has_field("eee") );
        CHECK("{ \"aaa\": \"qwerty\", \"ccc\": -100500, "
                "\"ddd\": true }" == o.serialize());
        o.add_str_field("ccc", "xyz");
        JsonObject ddd = JsonObject::new_object();
        ddd.add_typed_field("eee", "b:false");
        o.add_field("ddd", ddd); 
        CHECK("{ \"aaa\": \"qwerty\", \"ccc\": \"xyz\", "
                "\"ddd\": { \"eee\": false } }" == o.serialize());
    }
}

TEST_CASE( "Some tests for SHA256", "[full][hash]" ) {
    int hex_mode = HEX_LOWERCASE|HEX_NOSPACES;
    SECTION( "basic SHA256" ) {
        CHECK( "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
                == string_to_hexstring(sha256_digest(""), hex_mode) );
        CHECK( "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"
                == string_to_hexstring(sha256_digest("a"), hex_mode) );
        CHECK( "e7e8b89c2721d290cc5f55425491ecd6831355e91063f20b39c22f9ec6a71f91"
                == string_to_hexstring(sha256_digest("ABCDEFGHIJKLMNOP"), hex_mode) );
        CHECK( "0fe126d1ae30c66671c56b7895aa1edca4f0794e27f99e374260524196efb53f"
                == string_to_hexstring(sha256_digest(
                        "e7e8b89c2721d290cc5f55425491ecd6831355e91063f20b39c22f9ec6a71f91"), hex_mode) );
    }
    SECTION( "SHA256 HMAC" ) {
        CHECK( "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad"
                == string_to_hexstring(hmac_sha256_digest("", ""), hex_mode) );
        CHECK( "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"
                == string_to_hexstring(hmac_sha256_digest("key",
                        "The quick brown fox jumps over the lazy dog"), hex_mode) );
        CHECK( "61396e328aa68db475d7a92b3fdbf515c5df49961654620ca4cd3827f546209f"
                == string_to_hexstring(hmac_sha256_digest(
                        "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8",
                        "The quick brown fox jumps over the lazy dog"), hex_mode) );
        CHECK( "deda1649198d64cbc74dfd47705ff1a1400b92f3cdf41d8e87f75c92e29f7494"
                == string_to_hexstring(hmac_sha256_digest(
                        "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8+",
                        "The quick brown fox jumps over the lazy dog"), hex_mode) );
    }
}

TEST_CASE( "Some tests for Luhn algorithm", "[full][luhn]" ) {
    std::vector<std::string> cases{
        "4556010003898458",
        "4024007142076149",
        "4024007104953392",
        "4485885429059249",
        "4024007148404196",
        "5276663720734368",
        "5150158941098827",
        "5580989627560239",
        "5587477196001895",
        "5134820490751921",
        "6011431373154641",
        "6011783564165038",
        "6011105839205565",
        "6011542291600273",
        "6011029693567516",
        "342614564327869",
        "375577692445517",
        "342542450952112",
        "344767932252017",
        "377426240334393",
    };

    for (auto i = cases.begin(); i != cases.end(); ++i) {
        const std::string &p = *i;
        CHECK(luhn_check(p.c_str(), p.size()));
        CHECK(p[p.size() - 1] == luhn_control_digit(p.c_str(), p.size() - 1));
        std::string q = p;
        q[q.size() - 1] = '0' + ((q[q.size() - 1] - '0' + 1) % 10);
        CHECK( ! luhn_check(q.c_str(), q.size()));
    }

    for (int i = 8; i < 32; ++i) {
        CHECK(luhn_check(generate_pan(i).c_str(), i));
    }
}

const int TEST_PORT = 52348;

typedef const HttpResponse (*TestHttpHandler)(const HttpRequest &request);

class TestHttpServer;

class BackgroundHttpServerThread: public Yb::Thread
{
    TestHttpServer *serv_;
    void on_run();
public:
    BackgroundHttpServerThread(TestHttpServer *serv) : serv_(serv) {}
};

class TestHttpServer: public HttpServer<TestHttpHandler>
{
    static const HttpResponse process(const HttpRequest &request)
    {
        int c = boost::lexical_cast<int>(request.params()["a"]) +
                boost::lexical_cast<int>(request.params()["b"]);
        HttpResponse resp(HTTP_1_0, 200, "Okay");
        resp.set_response_body("<c>" + boost::lexical_cast<std::string>(c)
                               + "</c>\n", "application/xyz");
        return resp;
    }

    static const HandlerMap mk_handlers()
    {
        HandlerMap m;
        m["/process"] = TestHttpServer::process;
        return m;
    }

public:
    TestHttpServer():
        HttpServer("127.0.0.1", TEST_PORT, 1, mk_handlers(), NULL)
    {}

    void start()
    {
        BackgroundHttpServerThread *thr = new BackgroundHttpServerThread(this);
        thr->start();
    }
};

void BackgroundHttpServerThread::on_run() { serv_->serve(); }

TEST_CASE( "Test HTTP server/client", "[full][http]" ) {

    TestHttpServer serv;
    try {
        serv.bind();
    }
    catch (...) {}
    REQUIRE( serv.is_bound() );
    serv.start();
    sleep(1);
    REQUIRE( serv.is_serving() );

    HttpResponse r = http_post("http://127.0.0.1:" +
                               boost::lexical_cast<std::string>(TEST_PORT) +
                               "/process?b=11&a=31",
                               HTTP_POST_NO_LOGGER, 0, "GET");

    CHECK( 200 == r.resp_code() );
    CHECK( "Okay" == r.resp_desc() );
    CHECK( "application/xyz" == r.get_header("content-type") );
    CHECK( 10 == r.get_content_length() );
    CHECK( "<c>42</c>\n" == r.body() );
}

TEST_CASE( "Test replace_str correctness", "[full][replace_str]" ) {
    CHECK( "" == replace_str("", "", "") );
    CHECK( "abc" == replace_str("abc", "x", "y") );
    CHECK( "ac" == replace_str("abc", "b", "") );
    CHECK( "acd" == replace_str("abcbd", "b", "") );
    CHECK( "a" == replace_str("abb", "b", "") );
    CHECK( "b" == replace_str("a", "a", "b") );
    CHECK( "bb" == replace_str("aa", "a", "b") );
    CHECK( "bbc" == replace_str("aac", "a", "b") );
    CHECK( "cbb" == replace_str("caa", "a", "b") );
    CHECK( "axy" == replace_str("abc", "bc", "xy") );
    CHECK( "xybxyc" == replace_str("abac", "a", "xy") );
    CHECK( "xyxy" == replace_str("acac", "ac", "xy") );
    CHECK( "xac" == replace_str("acacac", "acac", "x") );
    CHECK( "x" == replace_str("acac", "acac", "x") );
}

TEST_CASE( "Test get_utc_iso_ts", "[full][utc_ts]" ) {
    const auto ts = get_utc_iso_ts();
    CHECK( Yb::StrUtils::starts_with(ts, "20") );
    CHECK( ts.size() == strlen("2016-08-30 12:35:00") );
}

// vim:ts=4:sts=4:sw=4:et:
