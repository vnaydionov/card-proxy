// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_secvault.h"
#include "dek_pool.h"
#include "app_class.h"
#include "servant_utils.h"
#include "card_crypter.h"
#include "aes_crypter.h"
#include <boost/lexical_cast.hpp>
#include "domain/VaultUser.h"
#include "domain/SecureVault.h"

class AuthError: public std::runtime_error {
public:
    AuthError()
        : runtime_error("can't authenticate/authorize user")
    {}
};

namespace LogicSecVault {

Domain::SecureVault check_token_against_domain(
        Yb::Session &session, const std::string &token, const std::string &domain)
{
    try {
        Domain::SecureVault item = Yb::query<Domain::SecureVault>(session)
            .filter_by(Domain::SecureVault::c.token_string == token &&
                       Domain::SecureVault::c.domain == domain)
            .one();
        return item;
    }
    catch (const Yb::NoDataFound &) {
        throw TokenNotFound();
    }
}

Domain::VaultUser check_user(Yb::Session &session, Yb::ILogger &logger,
                const Yb::StringDict &params, const std::string &command)
{
    std::string user = params.get("user", "");
    std::string passwd = params.get("passwd", "");
    std::string domain = params.get("domain", "");
    try {
        YB_ASSERT(user.size() && passwd.size() && domain.size());
        std::string digest = string_to_hexstring(
            sha256_digest(passwd),  HEX_LOWERCASE | HEX_NOSPACES);
        Domain::VaultUser good_user = Yb::query<Domain::VaultUser>(session)
            .filter_by(Domain::VaultUser::c.login == user)
            .filter_by(Domain::VaultUser::c.passwd == digest)
            .filter_by(Domain::VaultUser::c.roles.like_(
                        Yb::ConstExpr("%[" + command + ":" + domain + "]%")))
            .one();
        return good_user;
    }
    catch (const std::exception &e) {
        logger.error("Can't authenticate/authorize user (" + user + ") "
                "to perform command (" + command + ") in domain (" + domain + ")"
                ":\n" + std::string(e.what()));
        throw AuthError();
    }
}

Yb::ElementTree::ElementPtr tokenize(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    try {
        Tokenizer tokenizer(theApp::instance().cfg(), logger, session, false);
        Domain::VaultUser user = check_user(session, logger, params, "tokenize");
        std::string domain = params.get("domain", "");
        std::string plain_text = params.get("plain_text", "");
        bool dedup = params.get("dedup", "") == "true";
        if (dedup) {
            try {
                std::string token = tokenizer.search(plain_text);
                if (!token.empty())
                {
                    Domain::SecureVault item = check_token_against_domain(
                            session, token, domain);
                    Yb::ElementTree::ElementPtr resp = mk_resp("success");
                    resp->sub_element("token", token);
                    return resp;
                }
            }
            catch (const TokenNotFound &) { }
        }
        Yb::DateTime expire_ts;
        std::string expire_ts_str = params.get("expire_ts", "");
        Yb::from_string(expire_ts_str, expire_ts);
        std::string token = tokenizer.tokenize(plain_text, expire_ts, false);
        session.flush();
        Domain::SecureVault item = Yb::query<Domain::SecureVault>(session)
            .filter_by(Domain::SecureVault::c.token_string == token)
            .one();
        item.creator = Domain::VaultUser::Holder(user);
        item.domain = params["domain"];
        item.comment = params.get("comment", "");
        item.notify_email = params.get("notify_email", "");
        Yb::ElementTree::ElementPtr resp = mk_resp("success");
        resp->sub_element("token", token);
        return resp;
    }
    catch (const AuthError &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "auth_error");
        return resp;
    }
    catch (const TokenNotFound &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "token_not_found");
        return resp;
    }
}

Yb::ElementTree::ElementPtr detokenize(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    try {
        Tokenizer tokenizer(theApp::instance().cfg(), logger, session, false);
        check_user(session, logger, params, "detokenize");
        std::string domain = params.get("domain", "");
        std::string token = params.get("token", "");
        Domain::SecureVault item = check_token_against_domain(session, token, domain);
        std::string plain_text = tokenizer.detokenize(token);
        Yb::ElementTree::ElementPtr resp = mk_resp("success");
        resp->sub_element("plain_text", plain_text);
        resp->sub_element("token", token);
        resp->sub_element("domain", domain);
        resp->sub_element("creator", item.creator->login);
        resp->sub_element("expire_ts", Yb::to_string(item.finish_ts.value()));
        std::string comment;
        if (!item.comment.is_null())
            comment = item.comment;
        resp->sub_element("comment", comment);
        std::string notify_email;
        if (!item.notify_email.is_null())
            notify_email = item.notify_email;
        resp->sub_element("notify_email", notify_email);
        return resp;
    }
    catch (const AuthError &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "auth_error");
        return resp;
    }
    catch (const TokenNotFound &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "token_not_found");
        return resp;
    }
}

Yb::ElementTree::ElementPtr search_token(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    try {
        Tokenizer tokenizer(theApp::instance().cfg(), logger, session, false);
        check_user(session, logger, params, "search_token");
        std::string domain = params.get("domain", "");
        std::string plain_text = params.get("plain_text", "");
        std::string token = tokenizer.search(plain_text);
        if (token.empty())
            throw TokenNotFound();
        Domain::SecureVault item = check_token_against_domain(session, token, domain);
        Yb::ElementTree::ElementPtr resp = mk_resp("success");
        resp->sub_element("plain_text", plain_text);
        resp->sub_element("token", token);
        resp->sub_element("domain", domain);
        resp->sub_element("creator", item.creator->login);
        resp->sub_element("expire_ts", Yb::to_string(item.finish_ts.value()));
        std::string comment;
        if (!item.comment.is_null())
            comment = item.comment;
        resp->sub_element("comment", comment);
        std::string notify_email;
        if (!item.notify_email.is_null())
            notify_email = item.notify_email;
        resp->sub_element("notify_email", notify_email);
        return resp;
    }
    catch (const AuthError &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "auth_error");
        return resp;
    }
    catch (const TokenNotFound &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "token_not_found");
        return resp;
    }
}

Yb::ElementTree::ElementPtr remove_token(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    try {
        Tokenizer tokenizer(theApp::instance().cfg(), logger, session, false);
        check_user(session, logger, params, "remove_token");
        std::string domain = params.get("domain", "");
        std::string token = params.get("token", "");
        check_token_against_domain(session, token, domain);
        bool success = tokenizer.remove_data_token(token);
        if (!success)
            throw TokenNotFound();
        Yb::ElementTree::ElementPtr resp = mk_resp("success");
        resp->sub_element("token", token);
        resp->sub_element("domain", domain);
        return resp;
    }
    catch (const AuthError &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "auth_error");
        return resp;
    }
    catch (const TokenNotFound &) {
        Yb::ElementTree::ElementPtr resp = mk_resp("error", "token_not_found");
        return resp;
    }
}

} // LogicSecVault
// vim:ts=4:sts=4:sw=4:et:
