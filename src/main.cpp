// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "helpers.h"
#include "utils.h"
#include "aes_crypter.h"
#include "card_crypter.h"
#include "dek_pool.h"

#include "domain/Card.h"
#include "domain/Config.h"
#include "domain/DataKey.h"
#include "domain/IncomingRequest.h"

#include <util/util_config.h>
#include <util/string_utils.h>
#include <util/element_tree.h>

#include <pplx/pplx.h>
#include <cpprest/http_client.h>

#if defined(YBUTIL_WINDOWS)
#include <rpc.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
#if defined(YB_USE_WX)
#include <wx/app.h>
#elif defined(YB_USE_QT)
#include <QCoreApplication>
#endif

using namespace Domain;
using namespace std;
using namespace Yb;

CardData generate_random_card_data()
{
    CardData d;
    d["card_holder"] = generate_random_string(10) + " " +
                       generate_random_string(10);
    d["pan"] = generate_random_number(16);  // this will not pass the Luhn algorithm check
    d["cvn"] = generate_random_number(3);
    d["pan_masked"] = mask_pan(d["pan"]);
    d["expire_year"] = std::to_string(2018 + rand() % 5);
    d["expire_month"] = std::to_string(1 + rand() % 12);
    return d;
}

ElementTree::ElementPtr mk_resp(const string &status,
        const string &status_code = "")
{
    ElementTree::ElementPtr res = ElementTree::new_element("response");
    res->sub_element("status", status);
    if (!status_code.empty())
        res->sub_element("status_code", status_code);
    char buf[40];
    MilliSec t = get_cur_time_millisec();
    sprintf(buf, "%u.%03u", (unsigned)(t/1000), (unsigned)(t%1000));
    res->sub_element("ts", buf);
    return res;
}

Yb::LongInt
get_random()
{
    Yb::LongInt buf;
#if defined(__WIN32__) || defined(_WIN32)
    UUID new_uuid;
    UuidCreate(&new_uuid);
    buf = new_uuid.Data1;
    buf <<= 32;
    Yb::LongInt buf2 = (new_uuid.Data2 << 16) | new_uuid.Data3;
    buf += buf2;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1)
        throw std::runtime_error("can't open /dev/urandom");
    if (read(fd, &buf, sizeof(buf)) != sizeof(buf)) {
        close(fd);
        throw std::runtime_error("can't read from /dev/urandom");
    }
    close(fd);
#endif
    return buf;
}


typedef ElementTree::ElementPtr (*XmlHttpHandler)(
        Session &session, ILogger &logger,
        const StringDict &params);

typedef const HttpHeaders (*SimpleHttpHandler)(
        ILogger &logger, const HttpHeaders &request);

class CardProxyHttpWrapper
{
    string name_, default_status_;
    XmlHttpHandler f_;
    SimpleHttpHandler g_;

    string dump_result(ILogger &logger, ElementTree::ElementPtr res)
    {
        string res_str = res->serialize();
        logger.info("result: " + res_str);
        return res_str;
    }

public:
    CardProxyHttpWrapper(): f_(NULL) {}
    CardProxyHttpWrapper(const string &name, XmlHttpHandler f,
            const string &default_status = "not_available")
        : name_(name), default_status_(default_status), f_(f), g_(NULL)
    {}
    CardProxyHttpWrapper(const string &name, SimpleHttpHandler g,
            const string &default_status = "not_available")
        : name_(name), default_status_(default_status), f_(NULL), g_(g)
    {}
    const string &name() const { return name_; }
    const HttpHeaders operator() (const HttpHeaders &request)
    {
        ILogger::Ptr logger(theApp::instance().new_logger(name_));
        TimerGuard t(*logger);
        try {
            const StringDict &params = request.get_params();
            logger->info("started path: " + request.get_path()
                         + ", params: " + dict2str(params));
            if (f_)
            {
                logger->info("started path: " + request.get_path()
                             + ", params: " + dict2str(params));
                //int version = params.get_as<int>("version");
                //YB_ASSERT(version >= 2);
                auto_ptr<Session> session(
                        theApp::instance().new_session());
                ElementTree::ElementPtr res = f_(*session, *logger, params);
                session->commit();
                t.set_ok();
                HttpHeaders response(10, 200, "OK");
                response.set_response_body(dump_result(*logger, res), "text/xml");
                return response;
            }
            else
            {
                HttpHeaders response = g_(*logger, request);
                return response;
            }
        }
        catch (const ApiResult &ex) {
            t.set_ok();
            HttpHeaders response(10, 200, "OK");
            response.set_response_body(dump_result(*logger, ex.result()), "text/xml");
            return response;
        }
        catch (const exception &ex) {
            logger->error(string("exception: ") + ex.what());
            HttpHeaders response(10, 200, "OK");
            response.set_response_body(dump_result(*logger, mk_resp(default_status_)), "text/xml");
            return response;
        }
    }
};

#define WRAP(func) CardProxyHttpWrapper(#func, func)

ElementTree::ElementPtr dek_status(Session &session, ILogger &logger,
        const StringDict &params)
{
    ElementTree::ElementPtr resp = mk_resp("success");
    //DEKPool *dek_pool = DEKPool::get_instance();
    DEKPool dek_pool(theApp::instance().cfg(), session);
    DEKPoolStatus dek_status = dek_pool.get_status();
    resp->sub_element("total_count", Yb::to_string(dek_status.total_count));
    resp->sub_element("active_count", Yb::to_string(dek_status.active_count));
    resp->sub_element("use_count", Yb::to_string(dek_status.use_count));
    return resp;
}

ElementTree::ElementPtr dek_get(Session &session, ILogger &logger,
        const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    //DEKPool *dek_pool = DEKPool::get_instance();
    DEKPool dek_pool(theApp::instance().cfg(), session);
    DataKey data_key = dek_pool.get_active_data_key();
    resp->sub_element("ID", Yb::to_string(data_key.id.value()));
    resp->sub_element("DEK", data_key.dek_crypted);
    resp->sub_element("START_TS", Yb::to_string(data_key.start_ts.value()));
    resp->sub_element("FINISH_TS", Yb::to_string(data_key.finish_ts.value()));
    resp->sub_element("COUNTER", Yb::to_string(data_key.counter.value()));
    return resp;
}

ElementTree::ElementPtr dek_list(Session &session, ILogger &logger,
        const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    std::string master_key = CardCrypter::assemble_master_key(
        theApp::instance().cfg(), session);
    AESCrypter aes_crypter(master_key);
    auto data_keys = Yb::query<Domain::DataKey>(session)
            .filter_by(Domain::DataKey::c.counter < 10)
            .all();
    for (auto &data_key : data_keys) {
        ElementTree::ElementPtr dk = resp->sub_element("data_key");
        std::string dek_crypted = data_key.dek_crypted.value();
        std::string dek_decoded = decode_base64(dek_crypted);
        std::string dek_decrypted = string_to_hexstring(aes_crypter.decrypt(dek_decoded));
        dk->sub_element("id", Yb::to_string(data_key.id.value()));
        dk->sub_element("dek", dek_decrypted);
        dk->sub_element("counter", Yb::to_string(data_key.counter.value()));
        dk->sub_element("start_ts", Yb::to_string(data_key.start_ts.value()));
        dk->sub_element("finish_ts", Yb::to_string(data_key.finish_ts.value()));
    }
    return resp;
}

ElementTree::ElementPtr get_token(Session &session, ILogger &logger,
        const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    CardData card_data;
    std::string mode = params.get("mode", "");
    if (mode == "auto")
        card_data = generate_random_card_data();
    else {
        std::vector<std::string> keys{
            "pan", "expire_year", "expire_month", "card_holder", "cvn",
        };
        for (const auto &key: keys)
            card_data[key] = params.get(key, "");
    }
    //make norm check
    if (card_data.get("pan", "").empty())
        return mk_resp("error");

    CardCrypter card_crypter(theApp::instance().cfg(), session);
    CardData new_card_data = card_crypter.get_token(card_data);

    ElementTree::ElementPtr cd = resp->sub_element("card_data");
    for (const auto &p: new_card_data)
        cd->sub_element(p.first, p.second);
    return resp;
}

ElementTree::ElementPtr get_card(Session &session, ILogger &logger,
        const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    std::string token;
    try {
        token = params["token"];
    } catch(Yb::KeyError &err) {
    }

    CardCrypter card_crypter(theApp::instance().cfg(), session);
    CardData card_data = card_crypter.get_card(token);
    ElementTree::ElementPtr cd = resp->sub_element("card_data");
    for (const auto &p: card_data)
        cd->sub_element(p.first, p.second);
    return resp;
}

ElementTree::ElementPtr remove_card(Session &session, ILogger &logger, const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    CardCrypter card_crypter(theApp::instance().cfg(), session);
    std::string token = params["token"];
    card_crypter.remove_card(token);
    return resp;
}

ElementTree::ElementPtr remove_card_data(Session &session, ILogger &logger, const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    CardCrypter card_crypter(theApp::instance().cfg(), session);
    std::string token = params["token"];
    card_crypter.remove_card_data(token);
    return resp;
}

ElementTree::ElementPtr get_master_key(Session &session, ILogger &logger, const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    std::string master_key = CardCrypter::assemble_master_key(
        theApp::instance().cfg(), session);
    resp->sub_element("master_key", master_key);
    return resp;
}

ElementTree::ElementPtr set_master_key(Session &session, ILogger &logger, const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    CardCrypter card_crypter(theApp::instance().cfg(), session);
    std::string new_key = params["key"];
    card_crypter.change_master_key(new_key);
    return resp;
}


const HttpHeaders bind_card(ILogger &logger, const HttpHeaders &request)
{
    web::http::client::http_client client("http://localhost/myfile.json");  // check linking libcpprest

    HttpHeaders response(10, 200, "OK");
    response.set_response_body("{\"preved\": 111}", "application/json");
    return response;
}

const HttpHeaders start_payment(ILogger &logger, const HttpHeaders &request)
{
    HttpHeaders response(10, 302, "Redirect");
    response.set_header("Location", "http://localhost/");
    return response;
}


int main(int argc, char *argv[])
{
    CardProxyHttpWrapper handlers[] = {
        WRAP(dek_status),
        WRAP(dek_get),
        WRAP(dek_list),
        WRAP(get_token),
        WRAP(get_card),
        WRAP(remove_card),
        WRAP(remove_card_data),
        WRAP(set_master_key),
        WRAP(get_master_key),
        // proxy methods
        WRAP(bind_card),
        WRAP(start_payment),
    };
    int n_handlers = sizeof(handlers)/sizeof(handlers[0]);
    return run_server_app("settings.xml", handlers, n_handlers);
}

// vim:ts=4:sts=4:sw=4:et:
