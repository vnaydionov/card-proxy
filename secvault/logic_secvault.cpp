// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_secvault.h"
#include "dek_pool.h"
#include "app_class.h"
#include "servant_utils.h"
#include "card_crypter.h"
#include <boost/lexical_cast.hpp>

namespace LogicSecVault {

Yb::ElementTree::ElementPtr tokenize(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Tokenizer tokenizer(theApp::instance().cfg(), logger, session);
    std::string user = params.get("user", "");
    std::string passwd = params.get("passwd", "");
    YB_ASSERT(user != "" && passwd != "");
    std::string plain_text = params.get("plain_text", "");
    Yb::DateTime expire_ts;
    std::string expire_ts_str = params.get("expire_ts", "");
    Yb::from_string(expire_ts_str, expire_ts);
    std::string dedup = params.get("dedup", "");
    std::string token = tokenizer.tokenize(
            plain_text, expire_ts, dedup == "true");
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    resp->sub_element("token", token);
    return resp;
}

Yb::ElementTree::ElementPtr detokenize(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Tokenizer tokenizer(theApp::instance().cfg(), logger, session);
    std::string user = params.get("user", "");
    std::string passwd = params.get("passwd", "");
    YB_ASSERT(user != "" && passwd != "");
    std::string token = params.get("token", "");
    std::string plain_text = tokenizer.detokenize(token);
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    resp->sub_element("plain_text", plain_text);
    return resp;
}

Yb::ElementTree::ElementPtr search_token(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Tokenizer tokenizer(theApp::instance().cfg(), logger, session);
    std::string user = params.get("user", "");
    std::string passwd = params.get("passwd", "");
    YB_ASSERT(user != "" && passwd != "");
    std::string plain_text = params.get("plain_text", "");
    try {
        std::string token = tokenizer.search(plain_text);
        Yb::ElementTree::ElementPtr resp = mk_resp("success");
        resp->sub_element("token", token);
        return resp;
    }
    catch (const TokenNotFound &)
    {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "token_not_found");
        return resp;
    }
}

Yb::ElementTree::ElementPtr remove_token(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Tokenizer tokenizer(theApp::instance().cfg(), logger, session);
    std::string user = params.get("user", "");
    std::string passwd = params.get("passwd", "");
    YB_ASSERT(user != "" && passwd != "");
    std::string token = params.get("token", "");
    try {
        bool success = tokenizer.remove_data_token(token);
        if (success)
        {
            Yb::ElementTree::ElementPtr resp = mk_resp("success");
            return resp;
        }
        else {
            Yb::ElementTree::ElementPtr resp = mk_resp("error", "token_not_found");
            return resp;
        }
    }
    catch (const TokenNotFound &)
    {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "token_not_found");
        return resp;
    }
}

} // LogicSecVault
// vim:ts=4:sts=4:sw=4:et:
