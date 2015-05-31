// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "card_crypter.h"
#include "utils.h"
#include "aes_crypter.h"
#include "dek_pool.h"

#include "domain/DataKey.h"
#include "domain/Card.h"
#include "domain/IncomingRequest.h"

Yb::DateTime mk_exipre_dt(int expire_year, int expire_month)
{
    ++expire_month;
    if (expire_month > 12) {
        ++expire_year;
        expire_month = 1;
    }
    return Yb::dt_make(expire_year, expire_month, 1);
}

std::pair<int, int> split_exipre_dt(const Yb::DateTime &expire_dt)
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
    incoming_request.delete_object();
}

void CardCrypter::remove_card(const std::string &token)
{
    Domain::Card card = find_card_by_token(session_, token);
    remove_incoming_request(card);
    card.delete_object();
}

void CardCrypter::remove_card_data(const std::string &token)
{
    Domain::Card card = find_card_by_token(session_, token);
    remove_incoming_request(card);
}

void CardCrypter::change_master_key(const std::string &key)
{
    //save key
    auto deks = Yb::query<Domain::DataKey>(session_).all();
    for (auto &dek : deks) {
        std::string old_dek = decode_dek(dek.dek_crypted);
        std::string new_dek = encode_data(key, old_dek);
        dek.dek_crypted = new_dek;
    }
    session_.flush();
    master_key_ = key;
}

static Domain::Card save_card(CardCrypter &cr, const CardData &card_data)
{
    Yb::Session &session = cr.session();
    DEKPool dek_pool(session);
    Domain::DataKey data_key = dek_pool.get_active_data_key();
    std::string dek = cr.decode_dek(data_key.dek_crypted);
    Domain::Card card;
    card.ts = Yb::now();
    // TODO: generate some other token in case of collision
    card.card_token = cr.generate_card_token();
    card.card_holder = card_data["card_holder"];
    card.expire_dt = mk_exipre_dt(card_data.get_as<int>("expire_year"),
                                  card_data.get_as<int>("expire_month"));
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
    DEKPool dek_pool(session);
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
    Yb::DateTime expire_dt = mk_exipre_dt(card_data.get_as<int>("expire_year"),
                                          card_data.get_as<int>("expire_month"));
    Yb::DomainResultSet<Domain::Card> found_card_rs =
        Yb::query<Domain::Card>(session_)
            .filter_by(Domain::Card::c.pan_masked == pan_masked)
            .filter_by(Domain::Card::c.expire_dt >= expire_dt)  // a hack
            .all();
    std::vector<Domain::Card> found_cards;
    std::copy(found_card_rs.begin(), found_card_rs.end(),
              std::back_inserter(found_cards));
    for (auto &found_card : found_cards) {
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
    Domain::Card card = Yb::query<Domain::Card>(session_)
            .filter_by(Domain::Card::c.card_token == token)
            .one();
    std::string dek = decode_dek(card.dek->dek_crypted);
    CardData result;
    result["pan"] = bcd_decode(decode_data(dek, card.pan_crypted));
    std::pair<int, int> expire_pair = split_exipre_dt(card.expire_dt);
    result["expire_year"] = std::to_string(expire_pair.first);
    result["expire_month"] = std::to_string(expire_pair.second);
    result["card_holder"] = card.card_holder;
    Yb::DomainResultSet<Domain::IncomingRequest> request_rs =
        Yb::query<Domain::IncomingRequest>(session_)
            .filter_by(Domain::IncomingRequest::c.card_id == card.id)
            .all();
    if (request_rs.begin() != request_rs.end()) {
        Domain::IncomingRequest incoming_request = *request_rs.begin();
        std::string dek_cvn = decode_dek(incoming_request.dek->dek_crypted);
        result["cvn"] = bcd_decode(decode_data(dek_cvn,
                                               incoming_request.cvn_crypted));
    }
    return result;
}

std::string CardCrypter::assemble_master_key(Yb::Session &session)
{
    // make there UBER LOGIC FOR COMPOSE MASTER KEY
    return "12345678901234567890123456789012"; // 32 bytes
}

std::string CardCrypter::generate_card_token()
{
    return string_to_hexstring(generate_random_bytes(16),
                               HEX_LOWERCASE|HEX_NOSPACES);
}

std::string CardCrypter::encode_data(const std::string &dek, const std::string &data)
{
    AESCrypter data_encrypter(dek);
    return encode_base64(data_encrypter.encrypt(data));
}

std::string CardCrypter::decode_data(const std::string &dek, const std::string &data_crypted)
{
    AESCrypter data_encrypter(dek);
    return data_encrypter.decrypt(decode_base64(data_crypted));
}

// vim:ts=4:sts=4:sw=4:et:
