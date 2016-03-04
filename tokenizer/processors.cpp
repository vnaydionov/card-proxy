// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include <sstream>
#include <boost/lexical_cast.hpp>

#include "processors.h"
#include "card_crypter.h"
#include "app_class.h"
#include "servant_utils.h"
#include "json_object.h"

static const Yb::StringDict fix_inbound_params(Yb::ILogger &logger,
                                               const Yb::StringDict &params0)
{
    Yb::StringDict params = params0;
    std::string card_number = params.pop("card_number", "");
    std::string cvn = params.pop("cvn", "");
    if (!card_number.empty()) {
        // fill card_data from HTTP params
        CardData card_data(
                card_number,
                boost::lexical_cast<int>(params.get("expiration_year", "")),
                boost::lexical_cast<int>(params.get("expiration_month", "")),
                params.get("cardholder", ""),
                cvn);
        // perform the tokenization
        if (!card_data.is_fake_card()) {
            std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
            CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
            card_data = card_crypter.get_token(card_data);
            session->commit();
        }
        // fix HTTP params
        params["expiration_year"] = card_data.format_year();
        params["expiration_month"] = card_data.format_month();
        if (!card_data.card_token.empty()) {
            params["card_token"] = card_data.card_token;
            params["card_number_masked"] = card_data.pan_masked;
            if (!card_data.cvn_token.empty())
                params["cvn_token"] = card_data.cvn_token;
        }
        else {
            params["card_number"] = card_number;
            if (!cvn.empty())
                params["cvn"] = cvn;
        }
    }
    else if (!cvn.empty()) {
        CardData card_data;
        // fill card_data from HTTP params
        card_data.cvn = cvn;
        // perform the tokenization
        std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
        CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
        card_data = card_crypter.get_token(card_data);
        session->commit();
        // fix HTTP params
        params["cvn_token"] = card_data.cvn_token;
    }
    return params;
}

void json_extract_params(JsonObject &params_obj,
                         Yb::StringDict &card_fields)
{
    // iterate all over object's elements
    json_object_object_foreach(params_obj.get(), ckey, cval)
    {
        (void)cval;
        const std::string key = ckey;
        if (key == "card_number" || key == "cvn" ||
                key == "expiration_year" || key == "expiration_month" ||
                key == "cardholder")
        {
            card_fields[key] = params_obj.get_str_field(key);
            params_obj.delete_field(key);
        }
    }
}

const std::string bind_card__fix_json(Yb::ILogger &logger,
                                      const std::string &body)
{
    JsonObject obj = JsonObject::parse(body);
    JsonObject params_obj = obj.get_field("params");
    Yb::StringDict card_fields;
    json_extract_params(params_obj, card_fields);

    // perform the tokenization
    Yb::StringDict new_card_fields = fix_inbound_params(logger, card_fields);

    // fix JSON object to be forwarded
    for (auto i = new_card_fields.begin(), iend = new_card_fields.end();
            i != iend; ++i)
    {
        params_obj.add_str_field(i->first, i->second);
    }

    // serialize
    auto input_body_fixed =  obj.serialize();
    logger.debug("fixed body: " + input_body_fixed);
    return input_body_fixed;
}

const Yb::StringDict authorize__fix_params(Yb::ILogger &logger,
                                           const Yb::StringDict &params0)
{
    Yb::StringDict params = params0;
    std::string card_token = params.pop("card_token", "");
    std::string cvn_token = params.pop("cvn_token", "");
    if (!card_token.empty() || !cvn_token.empty()) {
        std::auto_ptr<Yb::Session> session(theApp::instance().new_session());
        CardCrypter card_crypter(theApp::instance().cfg(), logger, *session);
        CardData new_card_data = card_crypter.get_card(card_token, cvn_token);
        if (!card_token.empty())
            params["card_number"] = new_card_data.pan;
        if (!cvn_token.empty())
            params["cvn"] = new_card_data.cvn;
    }
    logger.debug("fixed params: " + dict2str(params));
    return params;
}

const Yb::StringDict start_payment__fix_params(Yb::ILogger &logger,
                                               const Yb::StringDict &params0)
{
    Yb::StringDict params = fix_inbound_params(logger, params0);
    logger.debug("fixed params: " + dict2str(params));
    return params;
}

// vim:ts=4:sts=4:sw=4:et:
