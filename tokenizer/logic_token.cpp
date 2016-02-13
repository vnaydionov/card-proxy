// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <json-c/json.h>

#include "logic_token.h"
#include "card_crypter.h"
#include "app_class.h"
#include "servant_utils.h"

static inline json_object *json_get_field(json_object *jobj,
                                          const std::string &key)
{
    json_object *field_jobj = json_object_object_get(jobj, key.c_str());
    if (!field_jobj)
        throw Yb::KeyError("JSON has no '" + key + "' key");
    return field_jobj;
}

static inline const std::string json_get_str_field(json_object *jobj,
                                                   const std::string &key)
{
    json_object *field_jobj = json_get_field(jobj, key);
    if (json_object_get_type(field_jobj) != json_type_string)
        throw Yb::ValueError("JSON key '" + key
                + "' expected to contain a string");
    return std::string(json_object_get_string(field_jobj));
}

static inline const std::string json_get_typed_field(json_object *jobj,
                                                     const std::string &key)
{
    json_object *field_jobj = json_get_field(jobj, key);
    enum json_type jtype = json_object_get_type(field_jobj);
    switch (jtype) {
        case json_type_boolean:
            return "b:" + std::string(
                    json_object_get_boolean(jobj)? "true": "false");
        case json_type_int:
            return "i:" + boost::lexical_cast<std::string>(
                    json_object_get_int(field_jobj));
        case json_type_double:
            return "f:" + boost::lexical_cast<std::string>(
                    json_object_get_double(field_jobj));
        case json_type_string:
            return "s:" + std::string(
                    json_object_get_string(field_jobj));
        default:
            break;
    }
    throw Yb::ValueError("JSON key '" + key
            + "' has unsupported type " + Yb::to_string((int)jtype));
}

static void json_put_field(json_object *jobj,
                           const std::string &key,
                           json_object *field_jobj)
{
    json_object_object_add(jobj, key.c_str(), field_jobj);
}

static void json_put_str_field(json_object *jobj,
                               const std::string &key,
                               const std::string &value)
{
    json_object *jstr = json_object_new_string(value.c_str());
    if (!jstr)
        throw Yb::RunTimeError("can't allocate a JSON object");
    json_put_field(jobj, key, jstr);
}

static void json_put_typed_field(json_object *jobj,
                                 const std::string &key,
                                 const std::string &value)
{
    json_object *field_jobj = NULL;
    if (value.size() >= 2 && value[1] == ':')
        switch (value[0]) {
            case 'b':
                field_jobj = json_object_new_boolean(value.substr(2) == "true");
                break;
            case 'i':
                field_jobj = json_object_new_int(
                        boost::lexical_cast<int>(value.substr(2)));
                break;
            case 'f':
                field_jobj = json_object_new_double(
                        boost::lexical_cast<double>(value.substr(2)));
                break;
            case 's':
                field_jobj = json_object_new_string(value.substr(2).c_str());
                break;
            default:
                break;
        }
    if (!field_jobj)
        throw Yb::RunTimeError("can't allocate a JSON object or wrong type");
    json_put_field(jobj, key, field_jobj);
}

const std::string bind_card__fix_json(Yb::ILogger &logger,
                                      const std::string &body)
{
    json_object *jobj = json_tokener_parse(body.c_str());
    if (!jobj)
        throw Yb::ValueError("failed to parse JSON");

    try {
        json_object *params_jobj = json_get_field(jobj, "params");
        // fill card_data from the JSON object
        CardData card_data(
            json_get_str_field(params_jobj, "card_number"),
            boost::lexical_cast<int>(
                json_get_str_field(params_jobj, "expiration_year")),
            boost::lexical_cast<int>(
                json_get_str_field(params_jobj, "expiration_month")),
            json_get_str_field(params_jobj, "cardholder")
        );
        try {
            card_data.cvn = json_get_str_field(params_jobj, "cvn");
        }
        catch (const Yb::KeyError &) {
            // do nothing
        }
        Yb::StringDict misc_fields;
        json_object_object_foreach(params_jobj, key, val) {
            // iterate over all object's elements
            if (!(
                    !strcmp(key, "card_number") ||
                    !strcmp(key, "expiration_year") ||
                    !strcmp(key, "expiration_month") ||
                    !strcmp(key, "cardholder") ||
                    !strcmp(key, "cvn")
                ))
            {
                misc_fields[key] = json_get_typed_field(params_jobj, key);
            }
        }
        json_object_put(jobj);
        jobj = params_jobj = NULL;

        // perform the tokenization
        if (!card_data.is_fake_card_trust()) {
            std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
            CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
            card_data = card_crypter.get_token(card_data);
            session->commit();
        }

        // fill new JSON object to be forwarded
        jobj = json_object_new_object();
        if (!jobj)
            throw Yb::RunTimeError("can't allocate a JSON object");
        params_jobj = json_object_new_object();
        if (!params_jobj)
            throw Yb::RunTimeError("can't allocate a JSON object");
        json_object_object_add(jobj, "params", params_jobj);

        json_put_str_field(params_jobj, "expiration_year",
                           card_data.format_year());
        json_put_str_field(params_jobj, "expiration_month",
                           card_data.format_month());
        json_put_str_field(params_jobj, "cardholder",
                           card_data.card_holder);
        if (!card_data.card_token.empty()) {
            json_put_str_field(params_jobj, "card_number_masked",
                               card_data.pan_masked);
            json_put_str_field(params_jobj, "card_token",
                               card_data.card_token);
            if (!card_data.cvn_token.empty())
                json_put_str_field(params_jobj, "cvn_token",
                                   card_data.cvn_token);
        }
        else {
            json_put_str_field(params_jobj, "card_number",
                               card_data.pan);
            if (!card_data.cvn.empty())
                json_put_str_field(params_jobj, "cvn",
                                   card_data.cvn);
        }
        auto i = misc_fields.begin(), iend = misc_fields.end();
        for (; i != iend; ++i)
            json_put_typed_field(params_jobj, i->first, i->second);

        // serialize
        std::string input_body_fixed(json_object_to_json_string(jobj));
        json_object_put(jobj);
        jobj = params_jobj = NULL;
        logger.debug("fixed body: " + input_body_fixed);
        return input_body_fixed;
    }
    catch (...) {
        if (jobj)
            json_object_put(jobj);
        throw;
    }
}

const Yb::StringDict authorize__fix_params(Yb::ILogger &logger,
                                           const Yb::StringDict &params0)
{
    Yb::StringDict params = params0;
    std::string card_token = params.pop("card_token", "");
    std::string cvn_token = params.pop("cvn_token", "");
    if (!card_token.empty()) {
        std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
        CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
        CardData new_card_data = card_crypter.get_card(card_token, cvn_token);
        params["card_number"] = new_card_data.pan;
        params["cardholder"] = new_card_data.card_holder;
        params["expiration_year"] = new_card_data.format_year();
        params["expiration_month"] = new_card_data.format_month();
        if (!new_card_data.cvn.empty())
            params["cvn"] = new_card_data.cvn;
    }
    return params;
}

const Yb::StringDict start_payment__fix_params(Yb::ILogger &logger,
                                               const Yb::StringDict &params0)
{
    Yb::StringDict params = params0;
    if (!params.get("card_number", "").empty()) {
        // fill card_data from HTTP params
        CardData card_data(
                params.get("card_number", ""),
                boost::lexical_cast<int>(params.get("expiration_year", "")),
                boost::lexical_cast<int>(params.get("expiration_month", "")),
                params.get("cardholder", ""));
        if (!params.get("cvn", "").empty())
            card_data.cvn = params.get("cvn", "");
        // perform the tokenization
        if (!card_data.is_fake_card_trust()) {
            std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
            CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
            card_data = card_crypter.get_token(card_data);
            session->commit();
            // fix HTTP params
            params.pop("card_number", "");
            params.pop("cvn", "");
        }
        if (!card_data.card_token.empty()) {
            params["card_number_masked"] = card_data.pan_masked;
            params["card_token"] = card_data.card_token;
            if (!card_data.cvn_token.empty())
                params["cvn_token"] = card_data.cvn_token;
        }
    }
    else if (!params.get("cvn", "").empty()) {
        CardData card_data;
        // fill card_data from HTTP params
        card_data.cvn = params.get("cvn", "");
        // perform the tokenization
        std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
        CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
        card_data = card_crypter.get_token(card_data);
        session->commit();
        // fix HTTP params
        params.pop("cvn", "");
        params["cvn_token"] = card_data.cvn_token;
    }
    logger.debug("fixed params: " + dict2str(params));
    return params;
}

// vim:ts=4:sts=4:sw=4:et:
