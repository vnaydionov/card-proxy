// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "card_crypter.h"
#include "utils.h"
#include "aes_crypter.h"
#include "dek_pool.h"

#include "domain/DataKey.h"
#include "domain/Config.h"
#include "domain/Card.h"
#include "domain/IncomingRequest.h"

static void send_key_to_server(const std::string &key);
static std::string recv_key_from_server(const std::string &host, int port);

Yb::DateTime mk_expire_dt(int expire_year, int expire_month)
{
    ++expire_month;
    if (expire_month > 12) {
        ++expire_year;
        expire_month = 1;
    }
    return Yb::dt_make(expire_year, expire_month, 1);
}

std::pair<int, int> split_expire_dt(const Yb::DateTime &expire_dt)
{
    int expire_year = Yb::dt_year(expire_dt);
    int expire_month = Yb::dt_month(expire_dt);
    --expire_month;
    if (expire_month < 1) {
        --expire_year;
        expire_month = 12;
    }
    return std::make_pair(expire_year, expire_month);
}

static Domain::Card find_card_by_token(Yb::Session &session, const std::string &token)
{
    Domain::Card card;
    try {
        card = Yb::query<Domain::Card>(session)
                //.select_from<Domain::Card>()
                //.join<Domain::IncomingRequest>(Domain::Card::c.id ==
                //        Domain::IncomingRequest::c::card_id)
                .filter_by(Domain::Card::c.card_token == token)
                .one();
    }
    catch (const Yb::NoDataFound &err) {
    }
    return card;
}

static void remove_incoming_request(Domain::Card &card)
{
    Domain::IncomingRequest incoming_request = *card.cvn;
    if (!incoming_request.is_empty())
        incoming_request.delete_object();
}

void CardCrypter::remove_card(const std::string &token)
{
    Domain::Card card = find_card_by_token(session_, token);
    if (!card.is_empty()) {
        remove_incoming_request(card);
        card.delete_object();
    }
}

void CardCrypter::remove_card_data(const std::string &token)
{
    Domain::Card card = find_card_by_token(session_, token);
    if (!card.is_empty())
        remove_incoming_request(card);
}

void CardCrypter::change_master_key(const std::string &key)
{
    config_->reload();
    auto deks = Yb::query<Domain::DataKey>(session_).all();
    for (auto &dek : deks) {
        std::string old_dek = decode_dek(dek.dek_crypted);
        std::string new_dek = encode_data(key, old_dek);
        dek.dek_crypted = new_dek;
    }

    // 16 bytes to server
    send_key_to_server(encode_base64(key.substr(0, 16)));
    // 8 bytes to config
    //app_settings.set_key(encode_base64(key.substr(16, 8)));
    //app_settings.save_to_xml();
    // 8 bytes to database
    Domain::Config config;
    try {
        config = Yb::query<Domain::Config>(session_).one();
        config.cvalue = encode_base64(key.substr(24, 8));
        config.update_ts = Yb::now();
    } catch (Yb::NoDataFound &err) { 
        config.ckey = "1234";
        config.cvalue = encode_base64(key.substr(24, 8));
        config.update_ts = Yb::now();
        config.save(session_);
    }
    session_.flush();
    master_key_ = key;
}

static Domain::Card save_card(CardCrypter &cr, const CardData &card_data)
{
    Yb::Session &session = cr.session();
    //DEKPool *dek_pool = DEKPool::get_instance();
    DEKPool dek_pool(cr.config(), session);
    Domain::DataKey data_key = dek_pool.get_active_data_key();
    std::string dek = cr.decode_dek(data_key.dek_crypted);
    Domain::Card card;
    card.ts = Yb::now();
    // TODO: generate some other token in case of collision
    card.card_token = cr.generate_card_token();
    card.card_holder = card_data["card_holder"];
    card.expire_year = card_data.get_as<int>("expire_year");
    card.expire_month = card_data.get_as<int>("expire_month");
    card.pan_crypted = cr.encode_data(dek, bcd_encode(card_data["pan"]));
    card.pan_masked = mask_pan(card_data["pan"]);
    card.dek = Domain::DataKey::Holder(data_key);
    card.save(session);
    data_key.counter = data_key.counter + 1;
    session.flush();
    return card;
}

static Domain::IncomingRequest save_cvn(CardCrypter &cr, const CardData &card_data,
                                        Domain::Card &card)
{
    Yb::Session &session = cr.session();
    //DEKPool *dek_pool = DEKPool::get_instance();
    DEKPool dek_pool(cr.config(), session);
    Domain::DataKey data_key = dek_pool.get_active_data_key();
    std::string dek = cr.decode_dek(data_key.dek_crypted);
    Domain::IncomingRequest incoming_request;
    incoming_request.ts = Yb::now();
    incoming_request.cvn_crypted = cr.encode_data(dek, bcd_encode(card_data["cvn"]));
    incoming_request.dek = Domain::DataKey::Holder(data_key);
    incoming_request.card = Domain::Card::Holder(card);
    incoming_request.save(session);
    data_key.counter = data_key.counter + 1;
    session.flush();
    return incoming_request;
}

CardData CardCrypter::get_token(const CardData &card_data)
{
    CardData result = card_data;
    result.pop("pan", "");
    result.pop("cvn", "");
    std::string pan_masked = mask_pan(card_data["pan"]);
    int expire_year = card_data.get_as<int>("expire_year");
    int expire_month = card_data.get_as<int>("expire_month");
    Yb::DomainResultSet<Domain::Card> found_card_rs =
        Yb::query<Domain::Card>(session_)
            .filter_by(Domain::Card::c.pan_masked == pan_masked)
            .filter_by(Domain::Card::c.expire_year == expire_year)
            .filter_by(Domain::Card::c.expire_month == expire_month)
            .all();
    for (auto &found_card : found_card_rs) {
        std::string dek = decode_dek(found_card.dek->dek_crypted);
        std::string pan = bcd_decode(decode_data(dek, found_card.pan_crypted));
        if (card_data["pan"] == pan) {
            if (!card_data.get("cvn", "").empty())
                save_cvn(*this, card_data, found_card);
            result["pan_masked"] = found_card.pan_masked;
            result["card_token"] = found_card.card_token;
            return result;
        }
    }
    Domain::Card card = save_card(*this, card_data);
    if (!card_data.get("cvn", "").empty())
        save_cvn(*this, card_data, card);
    result["pan_masked"] = card.pan_masked;
    result["card_token"] = card.card_token;
    return result;
}

CardData CardCrypter::get_card(const std::string &token)
{
    Domain::Card card;
    CardData card_data;
    try {
        card = Yb::query<Domain::Card>(session_)
            .filter_by(Domain::Card::c.card_token == token)
            .one();
    } catch(Yb::NoDataFound &err) {
        return card_data;
    }
    std::string dek = decode_dek(card.dek->dek_crypted);
    card_data["pan"] = bcd_decode(decode_data(dek, card.pan_crypted));
    card_data["expire_year"] = std::to_string(card.expire_year.value());
    card_data["expire_month"] = std::to_string(card.expire_month.value());
    card_data["card_holder"] = card.card_holder;
    Yb::DomainResultSet<Domain::IncomingRequest> request_rs =
        Yb::query<Domain::IncomingRequest>(session_)
            .filter_by(Domain::IncomingRequest::c.card_id == card.id)
            .all();
    if (request_rs.begin() != request_rs.end()) {
        Domain::IncomingRequest incoming_request = *request_rs.begin();
        std::string dek_cvn = decode_dek(incoming_request.dek->dek_crypted);
        card_data["cvn"] = bcd_decode(decode_data(dek_cvn,
                                               incoming_request.cvn_crypted));
    }
    return card_data;
}

static std::string recv_key_from_server(const std::string &host, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;
    std::string command = "get\n", buffer(100, ' ');
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        throw std::runtime_error("Socket error");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(host.c_str());
    server_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Connection error");
    }

    if (send(sockfd, command.data(), command.size(), 0) < 0) {
        close(sockfd);
        throw std::runtime_error("Send failed");
    }
    
    if (recv(sockfd, &buffer[0], buffer.size(), 0) < 0) {
        close(sockfd);
        throw std::runtime_error("Recv failed");
    }

    size_t pos = buffer.find(' ');
    return buffer.substr(4, pos - 4);
}

static void send_key_to_server(const std::string &key) {
    int sockfd;
    struct sockaddr_in server_addr;
    std::string command = "set key=" + key + "\n";
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        throw std::runtime_error("Socket error");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(4009);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Connection error");
    }

    if (send(sockfd, command.data(), command.size(), 0) < 0) {
        close(sockfd);
        throw std::runtime_error("Send failed");
    }
}

const std::string CardCrypter::assemble_master_key(IConfig *config, Yb::Session &session)
{
    config->reload();
    std::string server_key, config_key, database_key;
    try {
        // 16 bytes from server
        std::string key_keeper_host = config->get_value("key_keeper/host");
        int key_keeper_port = config->get_value_as_int("key_keeper/port");
        server_key = decode_base64(recv_key_from_server(key_keeper_host, key_keeper_port));
        // 8 bytes from config_file
        config_key = decode_base64(config->get_value("key_settings/kek2"));
        // 8 bytes from database
        Domain::Config config = Yb::query<Domain::Config>(session)
            .filter_by(Domain::Config::c.ckey == "KEK3").one();
        database_key = decode_base64(config.cvalue);
    } catch (std::runtime_error &err) {
        // TODO: proper logging
        return std::string();
    }
    return server_key + config_key + database_key;
}

const std::string CardCrypter::generate_card_token()
{
    return string_to_hexstring(generate_random_bytes(16),
                               HEX_LOWERCASE|HEX_NOSPACES);
}

const std::string CardCrypter::encode_data(const std::string &dek, const std::string &data)
{
    AESCrypter data_encrypter(dek);
    return encode_base64(data_encrypter.encrypt(data));
}

const std::string CardCrypter::decode_data(const std::string &dek, const std::string &data_crypted)
{
    AESCrypter data_encrypter(dek);
    return data_encrypter.decrypt(decode_base64(data_crypted));
}

// vim:ts=4:sts=4:sw=4:et:
