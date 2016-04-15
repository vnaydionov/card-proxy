// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_debug.h"
#include "dek_pool.h"
#include "app_class.h"
#include "servant_utils.h"
#include <boost/lexical_cast.hpp>

CardData generate_random_card_data()
{
    const std::string pan = generate_pan(16);
    YB_ASSERT(luhn_check(pan.c_str(), pan.size()));
    unsigned year = 0;
    const Yb::DateTime now = Yb::now();
    generate_random_bytes(&year, sizeof(year));
    year = Yb::dt_year(now) + 2 + year % 5;
    unsigned month = 0;
    generate_random_bytes(&month, sizeof(month));
    month = 1 + month % 12;
    const std::string name =
        generate_random_string(10) + " " + generate_random_string(10);
    const std::string cvn = generate_random_number(3);
    CardData d(pan, year, month, name, cvn);
    return d;
}

void write_card_data_to_xml(const CardData &card_data,
                            Yb::ElementTree::ElementPtr cd)
{
    cd->sub_element("expire_year", card_data.format_year());
    cd->sub_element("expire_month", card_data.format_month());
    cd->sub_element("card_holder", card_data.card_holder);
    if (!card_data.card_token.empty()) {
        cd->sub_element("pan_masked", card_data.pan_masked);
        cd->sub_element("card_token", card_data.card_token);
        if (!card_data.cvn_token.empty())
            cd->sub_element("cvn_token", card_data.cvn_token);
    }
    else {
        cd->sub_element("pan", card_data.pan);
        if (!card_data.cvn.empty())
            cd->sub_element("cvn", card_data.cvn);
    }
}

namespace LogicDebug {

Yb::ElementTree::ElementPtr debug_method(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    return resp;
}

Yb::ElementTree::ElementPtr dek_status(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
#ifdef TOKENIZER_CONFIG_SINGLETON
    TokenizerConfig &tcfg(theTokenizerConfig::instance().refresh());
#else
    TokenizerConfig tcfg;
#endif
    DEKPool dek_pool(theApp::instance().cfg(), logger, session,
                     tcfg.get_active_master_key(),
                     tcfg.get_active_master_key_version());
    DEKPoolStatus dek_status = dek_pool.get_status();
    resp->sub_element("total_count", Yb::to_string(dek_status.total_count));
    resp->sub_element("active_count", Yb::to_string(dek_status.active_count));
    resp->sub_element("use_count", Yb::to_string(dek_status.use_count));
    return resp;
}

Yb::ElementTree::ElementPtr tokenize_card(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    CardData card_data;
    std::string mode = params.get("mode", "");
    if (mode == "auto")
        card_data = generate_random_card_data();
    else
        card_data = CardData(
                params.get("pan"),
                boost::lexical_cast<int>(params.get("expire_year")),
                boost::lexical_cast<int>(params.get("expire_month")),
                params.get("card_holder"),
                params.get("cvn", ""));
    //make norm check
    if (card_data.pan.empty())
        return mk_resp("error");

    CardCrypter card_crypter(theApp::instance().cfg(), logger, session);
    CardData new_card_data = card_crypter.get_token(card_data);

    Yb::ElementTree::ElementPtr cd = resp->sub_element("card_data");
    write_card_data_to_xml(new_card_data, cd);
    return resp;
}

Yb::ElementTree::ElementPtr detokenize_card(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    std::string card_token, cvn_token;
    try {
        card_token = params["card_token"];
        cvn_token = params.get("cvn_token", "");
    }
    catch (const Yb::KeyError &) {
    }

    CardCrypter card_crypter(theApp::instance().cfg(), logger, session);
    CardData card_data = card_crypter.get_card(card_token, cvn_token);

    Yb::ElementTree::ElementPtr cd = resp->sub_element("card_data");
    write_card_data_to_xml(card_data, cd);
    return resp;
}

Yb::ElementTree::ElementPtr remove_card(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    CardCrypter card_crypter(theApp::instance().cfg(), logger, session);
    std::string card_token = params.get("card_token", "");
    if (!card_token.empty())
        card_crypter.remove_data_token(card_token);
    std::string cvn_token = params.get("cvn_token", "");
    if (!cvn_token.empty())
        card_crypter.remove_data_token(cvn_token);
    return resp;
}

Yb::ElementTree::ElementPtr run_load_scenario(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    int count = params.get_as<int>("count");
    YB_ASSERT(count > 0 && count < 1000000000);
    int failed_count = 0;
    long long t0 = Yb::get_cur_time_millisec();
    for (int i = 0; i < count; ++i) {
        try {
            CardData card_data = generate_random_card_data();
            Yb::StringDict params;
            params["expire_year"] = card_data.format_year();
            params["expire_month"] = card_data.format_month();
            params["card_holder"] = card_data.card_holder;
            params["pan"] = card_data.pan;
            params["cvn"] = card_data.cvn;
            Yb::ElementTree::ElementPtr r = tokenize_card(session, logger, params);
            std::string card_token = r->find_first("card_data/card_token")->get_text();
            std::string cvn_token = r->find_first("card_data/cvn_token")->get_text();
            YB_ASSERT(!card_token.empty() && !cvn_token.empty());
            Yb::StringDict params2;
            params2["card_token"] = card_token;
            params2["cvn_token"] = cvn_token;
            r = detokenize_card(session, logger, params2);
            YB_ASSERT(r->find_first("card_data/pan")->get_text() == card_data.pan);
            YB_ASSERT(r->find_first("card_data/expire_month")->get_text() == card_data.format_month());
            r = remove_card(session, logger, params2);
            YB_ASSERT(r->find_first("status")->get_text() == "success");
        }
        catch (const std::exception &e) {
            ++failed_count;
            logger.error(std::string("scenario failed: ") + e.what());
        }
    }
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    resp->sub_element("count", Yb::to_string(count));
    resp->sub_element("failed_count", Yb::to_string(failed_count));
    resp->sub_element("time", Yb::to_string(Yb::get_cur_time_millisec() - t0));
    return resp;
}

} // LogicDebug
// vim:ts=4:sts=4:sw=4:et:
