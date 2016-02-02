// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <cpprest/json.h>

#include "logic_token.h"
#include "card_crypter.h"
#include "app_class.h"
#include "servant_utils.h"

const std::string bind_card__fix_json(Yb::ILogger &logger,
                                      const std::string &body)
{
    // get request body as a JSON object with "params" attribute
    web::json::value input_json;
    std::istringstream inp(body);
    inp >> input_json;
    web::json::value &params = input_json.as_object().at("params");
    // fill card_data from the JSON object
    CardData card_data(
            params.at("card_number").as_string(),
            boost::lexical_cast<int>(params.at("expiration_year")
                                     .as_string()),
            boost::lexical_cast<int>(params.at("expiration_month")
                                     .as_string()),
            params.at("cardholder").as_string()
    );
    if (params.as_object().find("cvn") != params.as_object().end())
        card_data.cvn = params.at("cvn").as_string();
    // perform the tokenization
    if (!card_data.is_fake_card_trust()) {
        std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
        CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
        card_data = card_crypter.get_token(card_data);
        session->commit();
    }
    // fill new JSON object to be forwarded
    web::json::value params_fixed;
    params_fixed["expiration_year"] =
        web::json::value(card_data.format_year());
    params_fixed["expiration_month"] =
        web::json::value(card_data.format_month());
    params_fixed["cardholder"] = web::json::value(card_data.card_holder);
    if (!card_data.card_token.empty()) {
        params_fixed["card_number_masked"] =
            web::json::value(card_data.pan_masked);
        params_fixed["card_token"] =
            web::json::value(card_data.card_token);
        if (!card_data.cvn_token.empty())
            params_fixed["cvn_token"] = web::json::value(card_data.cvn_token);
    }
    else {
        params_fixed["card_number"] = web::json::value(card_data.pan);
        if (!card_data.cvn.empty())
            params_fixed["cvn"] = web::json::value(card_data.cvn);
    }
    // append specific fields
    params_fixed["token"] = params.at("token");
    if (params.as_object().find("region_id") != params.as_object().end())
        params_fixed["region_id"] = params["region_id"];
    // serialize into a new JSON document
    web::json::value input_json_fixed;
    input_json_fixed["params"] = params_fixed;
    std::string input_body_fixed = input_json_fixed.serialize();
    logger.debug("fixed body: " + input_body_fixed);
    return input_body_fixed;
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
