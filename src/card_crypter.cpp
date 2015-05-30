#include <string>

#include "card_crypter.h"
#include "dek_pool.h"
#include "helpers.h"


CardCrypter::CardCrypter(Yb::Session &session) : session(session) {
    _master_key = assemble_master_key();
}

CardCrypter::~CardCrypter() {
}

Domain::Card CardCrypter::get_token(const CardData &card_data) {
    AESCrypter master_crypter(_master_key);

    Domain::Card card = _save_card(master_crypter, card_data);
    Domain::IncomingRequest incoming_request = _save_cvn(master_crypter,
        card, card_data._cvn);

    return card;
}

Domain::Card CardCrypter::_save_card(AESCrypter &master_crypter, const CardData &card_data) {
    Domain::Card card;
    AESCrypter data_crypter;
    DEKPool dek_pool(session);
    Domain::DataKey data_key = dek_pool.get_active_data_key();
    std::string pure_dek = _get_decoded_dek(master_crypter, data_key.dek_crypted);
    data_crypter.set_key(pure_dek);
    std::string crypted_pan = _get_encoded_str(data_crypter, card_data._pan);

    card.card_token = _generate_card_token();
    card.ts = Yb::now();
    card.expire_dt = Yb::dt_make(2020, 12, 31); //TODO
    card.card_holder = card_data._chname;
    card.pan_masked = card_data._masked_pan;
    card.pan_crypted = crypted_pan;
    card.dek = Domain::DataKey::Holder(data_key);
    card.save(session);
    data_key.counter = data_key.counter + 1; session.commit();
    return card;
}

Domain::IncomingRequest CardCrypter::_save_cvn(AESCrypter &master_crypter,
    Domain::Card &card, const std::string &cvn) {
    Domain::IncomingRequest incoming_request;
    AESCrypter data_crypter;
    DEKPool dek_pool(session);
    Domain::DataKey data_key = dek_pool.get_active_data_key();
    std::string pure_dek = _get_decoded_dek(master_crypter, data_key.dek_crypted);
    data_crypter.set_key(pure_dek);
    std::string crypted_cvn = _get_encoded_str(data_crypter, cvn);

    incoming_request.ts = Yb::now();
    incoming_request.cvn_crypted = crypted_cvn;
    incoming_request.dek = Domain::DataKey::Holder(data_key);
    incoming_request.card = Domain::Card::Holder(card);
    incoming_request.save(session);
    data_key.counter = data_key.counter + 1;
    session.commit();
    return incoming_request;
}

CardData CardCrypter::get_card(const std::string &token) {
    AESCrypter master_crypter(_master_key);
    DEKPool dek_pool(session);
    Domain::Card card = Yb::query<Domain::Card>(session)
            .filter_by(Domain::Card::c.card_token == token)
            .one();
    Domain::IncomingRequest incoming_request = *card.cvn;

    std::string pure_card_dek = _get_decoded_dek(master_crypter, card.dek->dek_crypted);
    std::string pure_cvn_dek = _get_decoded_dek(master_crypter, incoming_request.dek->dek_crypted);
    AESCrypter card_crypter(pure_card_dek), cvn_crypter(pure_cvn_dek);
    std::string pure_pan = _get_decoded_str(card_crypter, card.pan_crypted);
    std::string pure_cvn = _get_decoded_str(cvn_crypter, incoming_request.cvn_crypted);

    CardData card_data(card.card_holder.value(), pure_pan, "",
        Yb::to_string(card.expire_dt.value()), pure_cvn);
    return card_data;
}

std::string CardCrypter::_generate_card_token() {
    return generate_random_number(20);
}

std::string CardCrypter::_get_encoded_str(AESCrypter &crypter, const std::string &str) {
    std::string bindec_str, base64_str;
    try {
        bindec_str = BinDecConverter().encode(str);
        base64_str = encode_base64(crypter.encrypt(bindec_str));
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.to_string() << std::endl;
    }
    return base64_str;
}

std::string CardCrypter::_get_decoded_str(AESCrypter &crypter, const std::string &str) {
    std::string bindec_str, pure_str;
    try {
        bindec_str = crypter.decrypt(decode_base64(str));
        pure_str = BinDecConverter().decode(bindec_str);
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.to_string() << std::endl;
    }
    return pure_str;
}

std::string CardCrypter::_get_decoded_dek(AESCrypter &crypter, const std::string &dek) {
    std::string pure_dek;
    try {
        pure_dek = crypter.decrypt(decode_base64(dek));
    } catch(AESBlockSizeException &exc) {
        std::cout << exc.to_string() << std::endl;
    }
    return pure_dek;
}

std::string assemble_master_key() {
    // make there UBER LOGIC FOR COMPOSE MASTER KEY
    return "12345678901234567890123456789012"; // 32 bytes
}

CardData generate_random_card_data() {
    std::string chname = generate_random_string(10) + " " +
        generate_random_string(10);
    std::string pan = generate_random_number(16);
    std::string masked_pan = pan;
    std::string expdate = "2020-12-31T00:00:00";//Yb::dt_make(2020, 12, 31);
    std::string cvn = generate_random_number(3);
    return CardData(chname, pan, masked_pan, expdate, cvn);
}
