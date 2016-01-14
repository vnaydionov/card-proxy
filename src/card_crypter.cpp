// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "card_crypter.h"
#include "utils.h"
#include "aes_crypter.h"
#include "dek_pool.h"
#include "tcp_socket.h"

#include "domain/DataKey.h"
#include "domain/Config.h"
#include "domain/Card.h"
#include "domain/IncomingRequest.h"

int CardCrypter::remove_incoming_request(const std::string &cvn_token)
{
    try {
        Domain::IncomingRequest incoming_request = Yb::query<Domain::IncomingRequest>(session_)
                .filter_by(Domain::IncomingRequest::c.cvn_token == cvn_token)
                .one();
        incoming_request.delete_object();
    }
    catch (const Yb::NoDataFound &) {
        return 1;
    }
    return 0;
}

int CardCrypter::remove_card(const std::string &card_token)
{
    try {
        Domain::Card card = Yb::query<Domain::Card>(session_)
                .filter_by(Domain::Card::c.card_token == card_token)
                .one();
        card.delete_object();
    }
    catch (const Yb::NoDataFound &) {
        return 1;
    }
    return 0;
}

void CardCrypter::change_master_key(const std::string &key)
{
    config_.reload();
    auto deks = Yb::query<Domain::DataKey>(session_).all();
    for (auto &dek : deks) {
        std::string old_dek = decode_dek(dek.dek_crypted);
        std::string new_dek = encode_data(key, old_dek);
        dek.dek_crypted = new_dek;
    }

    // 16 bytes to server
    send_key_to_server(config_, encode_base64(key.substr(0, 16)));
    // 8 bytes to config
    //app_settings.set_key(encode_base64(key.substr(16, 8)));
    //app_settings.save_to_xml();
    // 8 bytes to database
    Domain::Config config_record;
    try {
        config_record = Yb::query<Domain::Config>(session_).one();
        config_record.cvalue = encode_base64(key.substr(24, 8));
        config_record.update_ts = Yb::now();
    }
    catch (const Yb::NoDataFound &) { 
        config_record.ckey = "1234";
        config_record.cvalue = encode_base64(key.substr(24, 8));
        config_record.update_ts = Yb::now();
        config_record.save(session_);
    }
    session_.flush();
    master_key_ = key;
}

Domain::Card CardCrypter::save_card(const CardData &card_data)
{
    Domain::DataKey data_key = dek_pool_.get_active_data_key();
    std::string dek = decode_dek(data_key.dek_crypted);
    Domain::Card card;
    card.ts = Yb::now();
    // TODO: generate some other token in case of collision
    card.card_token = generate_card_token();
    card.expire_year = card_data.expire_year;
    card.expire_month = card_data.expire_month;
    card.card_holder = card_data.card_holder;
    card.pan_masked = card_data.pan_masked;
    card.pan_crypted = encode_data(dek, bcd_encode(card_data.pan));
    card.dek = Domain::DataKey::Holder(data_key);
    card.save(session_);
    data_key.counter = data_key.counter + 1;
    if (data_key.counter >= dek_pool_.dek_use_count())
        data_key.finish_ts = card.ts;
    session_.flush();
    return card;
}

Domain::IncomingRequest CardCrypter::save_cvn(const CardData &card_data)
{
    Domain::DataKey data_key = dek_pool_.get_active_data_key();
    std::string dek = decode_dek(data_key.dek_crypted);
    Domain::IncomingRequest incoming_request;
    incoming_request.cvn_token = generate_cvn_token();
    incoming_request.ts = Yb::now();
    incoming_request.cvn_crypted = encode_data(dek, bcd_encode(card_data.cvn));
    incoming_request.dek = Domain::DataKey::Holder(data_key);
    incoming_request.save(session_);
    data_key.counter = data_key.counter + 1;
    if (data_key.counter >= dek_pool_.dek_use_count())
        data_key.finish_ts = incoming_request.ts;
    session_.flush();
    return incoming_request;
}

CardData CardCrypter::get_token(const CardData &card_data)
{
    CardData result = card_data;
    result.clear_sensitive_data();
    if (!card_data.cvn.empty()) {
        Domain::IncomingRequest incoming_request = save_cvn(card_data);
        result.cvn_token = incoming_request.cvn_token;
    }
    if (!card_data.pan.empty()) {
        // find possible duplicates
        Yb::DomainResultSet<Domain::Card> found_card_rs =
            Yb::query<Domain::Card>(session_)
                .select_from<Domain::Card>()
                .join<Domain::DataKey>()
                .filter_by(Domain::Card::c.pan_masked == card_data.pan_masked)
                .filter_by(Domain::Card::c.expire_year == card_data.expire_year)
                .filter_by(Domain::Card::c.expire_month == card_data.expire_month)
                .all();
        for (auto &found_card : found_card_rs) {
            std::string dek = decode_dek(found_card.dek->dek_crypted);
            std::string pan = bcd_decode(decode_data(dek, found_card.pan_crypted));
            session_.debug("pan_decryped=" + pan + ", pan=" + card_data.pan);
            if (card_data.pan == pan) {
                result.card_token = found_card.card_token;
                return result;
            }
        }
        Domain::Card card = save_card(card_data);
        result.card_token = card.card_token;
    }
    return result;
}

CardData CardCrypter::get_card(const std::string &card_token,
                               const std::string &cvn_token)
{
    Domain::Card card;
    try {
        card = Yb::query<Domain::Card>(session_)
            .select_from<Domain::Card>()
            .join<Domain::DataKey>()
            .filter_by(Domain::Card::c.card_token == card_token)
            .one();
    }
    catch (const Yb::NoDataFound &) {
        throw TokenNotFound();
    }
    std::string card_dek = decode_dek(card.dek->dek_crypted);
    CardData card_data(
        bcd_decode(decode_data(card_dek, card.pan_crypted)),
        card.expire_year,
        card.expire_month,
        card.card_holder);
    if (!cvn_token.empty()) {
        try {
            Domain::IncomingRequest incoming_request =
                Yb::query<Domain::IncomingRequest>(session_)
                .select_from<Domain::IncomingRequest>()
                .join<Domain::DataKey>()
                .filter_by(Domain::IncomingRequest::c.cvn_token == cvn_token)
                .one();
            std::string cvn_dek = decode_dek(incoming_request.dek->dek_crypted);
            card_data.cvn = bcd_decode(decode_data(cvn_dek,
                        incoming_request.cvn_crypted));
        }
        catch (const Yb::NoDataFound &) {
            throw TokenNotFound();
        }
    }
    return card_data;
}

const std::string CardCrypter::generate_data_token()
{
    return string_to_hexstring(generate_random_bytes(16),
                               HEX_LOWERCASE | HEX_NOSPACES);
}

const std::string CardCrypter::generate_card_token()
{
    while (true) {
        std::string card_token = generate_data_token();
        int count = Yb::query<Domain::Card>(session_)
                .filter_by(Domain::Card::c.card_token == card_token)
                .count();
        if (!count)  // TODO: too optimistic
            return card_token;
    }
}

const std::string CardCrypter::generate_cvn_token()
{
    while (true) {
        std::string cvn_token = generate_data_token();
        int count = Yb::query<Domain::IncomingRequest>(session_)
                .filter_by(Domain::IncomingRequest::c.cvn_token == cvn_token)
                .count();
        if (!count)  // TODO: too optimistic
            return cvn_token;
    }
}

const std::string CardCrypter::assemble_master_key(IConfig &config,
                                                   Yb::ILogger &logger,
                                                   Yb::Session &session)
{
    config.reload();
    std::string server_key, config_key, database_key;
    try {
        // 16 bytes from server
        server_key = decode_base64(recv_key_from_server(config));
        // 8 bytes from config_file
        config_key = decode_base64(config.get_value("KeySettings/KEK2"));
        // 8 bytes from database
        Domain::Config config_record = Yb::query<Domain::Config>(session)
            .filter_by(Domain::Config::c.ckey == "KEK3").one();
        database_key = decode_base64(config_record.cvalue);
    }
    catch (const std::exception &e) {
        logger.error("error assembling the master key: " +
                std::string(e.what()));
        throw;
    }
    std::string out = server_key + config_key + database_key;
    if (out.size() != 32)
        throw Yb::RunTimeError("invalid master key length: "
                + std::to_string(out.size()));
    out = sha256_digest(out);
    if (out.size() != 32)
        throw Yb::RunTimeError("invalid master key length: "
                + std::to_string(out.size()));
    logger.debug("master key assembled OK");
    return out;
}

const std::string CardCrypter::encode_data(const std::string &dek,
                                           const std::string &data)
{
    AESCrypter data_encrypter(dek);
    return encode_base64(data_encrypter.encrypt(data));
}

const std::string CardCrypter::decode_data(const std::string &dek,
                                           const std::string &data_crypted)
{
    AESCrypter data_encrypter(dek);
    return data_encrypter.decrypt(decode_base64(data_crypted));
}

const std::string CardCrypter::recv_key_from_server(IConfig &config) {
    TcpSocket sock(-1, config.get_value_as_int("KeyKeeper/Timeout"));
    sock.connect(config.get_value("KeyKeeper/Host"),
                 config.get_value_as_int("KeyKeeper/Port"));
    sock.write("get\n");
    std::string response = sock.readline();
    sock.close(true);
    std::string prefix = "key=";
    if (response.substr(0, 4) != prefix)
        throw RunTimeError("KeyKeeper protocol error");
    size_t pos = response.find(' ');
    if (std::string::npos == pos)
        throw RunTimeError("KeyKeeper protocol error");
    return response.substr(prefix.size(), pos - prefix.size());
}

void CardCrypter::send_key_to_server(IConfig &config, const std::string &key) {
    TcpSocket sock(-1, config.get_value_as_int("KeyKeeper/Timeout"));
    sock.connect(config.get_value("KeyKeeper/Host"),
                 config.get_value_as_int("KeyKeeper/Port"));
    sock.write("set key=" + key + "\n");
    sock.close(true);
}


// vim:ts=4:sts=4:sw=4:et:
