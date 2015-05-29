#include "helpers.h"
#include "domain/Card.h"
#include "domain/IncomingRequest.h"
#include "domain/DataKey.h"
#include "domain/Config.h"
#include "utils.h"
#include "crypt.h"
#include "aes_crypter.h"
#include "card_data.h"

#include <util/util_config.h>
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
#include <iostream>
#include <string>
#include <util/string_utils.h>
#include <util/element_tree.h>

using namespace Domain;
using namespace std;
using namespace Yb;

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


ElementTree::ElementPtr bind_card(Session &session, ILogger &logger,
        const StringDict &params)
{
    ElementTree::ElementPtr resp = mk_resp("success");
    Domain::Card card;
    card.card_token = generate_random_number(32);
    card.ts = Yb::now();
    card.pan = params["pan"];/*convert data to string*/
    card.expire_dt = Yb::dt_make(params.get_as<int>("expire_year"), params.get_as<int>("expire_month"), 1);/*convert string to date*/
    card.card_holder = params["card_holder"];
    std::string pan_m1 =  params["pan"].substr(0, 6);
    std::string pan_m2 =  params["pan"].substr(params["pan"].size() - 4, 4);
    card.pan_masked = pan_m1 + "****" + pan_m2;
    card.save(session);
    session.commit();
    int card_id = card.id;

    resp->sub_element("card_id", Yb::to_string(card_id));
    resp->sub_element("card_holder", card.card_holder);
    resp->sub_element("pan_masked", card.pan_masked);
    std::string expire_dtYear = Yb::to_string(params.get_as<string>("expire_year"));/*convert data to string*/
    std::string expire_dtMonth = Yb::to_string(params.get_as<string>("expire_month"));/*convert data to string*/
    std::string expire_dtCD = expire_dtMonth + "/" + expire_dtYear;
    resp->sub_element("expire.dt", expire_dtCD); 
    return resp;
}

typedef ElementTree::ElementPtr (*HttpHandler)(
        Session &session, ILogger &logger,
        const StringDict &params);

class CardProxyHttpWrapper
{
    string name_, default_status_;
    HttpHandler f_;
    string dump_result(ILogger &logger, ElementTree::ElementPtr res)
    {
        string res_str = res->serialize();
        logger.info("result: " + res_str);
        return res_str;
    }

public:
    CardProxyHttpWrapper(): f_(NULL) {}
    CardProxyHttpWrapper(const string &name, HttpHandler f,
            const string &default_status = "not_available")
        : name_(name), default_status_(default_status), f_(f)
    {}
    const string &name() const { return name_; }
    string operator() (const StringDict &params)
    {
        ILogger::Ptr logger(theApp::instance().new_logger(name_));
        TimerGuard t(*logger);
        try {
            logger->info("started, params: " + dict2str(params));
            //int version = params.get_as<int>("version");
            //YB_ASSERT(version >= 2);
            auto_ptr<Session> session(
                    theApp::instance().new_session());
            ElementTree::ElementPtr res = f_(*session, *logger, params);
            session->commit();
            t.set_ok();
            return dump_result(*logger, res);
        }
        catch (const ApiResult &ex) {
            t.set_ok();
            return dump_result(*logger, ex.result());
        }
        catch (const exception &ex) {
            logger->error(string("exception: ") + ex.what());
            return dump_result(*logger, mk_resp(default_status_));
        }
    }
};

#define WRAP(func) CardProxyHttpWrapper(#func, func)

ElementTree::ElementPtr dek_status(Session &session, ILogger &logger,
        const StringDict &params)
{
    ElementTree::ElementPtr resp = mk_resp("success");
    DEKPool dek_pool(session);
    DEKPoolStatus dek_status = dek_pool.get_status();
    resp->sub_element("total_count", Yb::to_string(dek_status.total_count));
    resp->sub_element("active_count", Yb::to_string(dek_status.active_count));
    resp->sub_element("use_count", Yb::to_string(dek_status.use_count));
    return resp;
}

ElementTree::ElementPtr dek_get(Session &session, ILogger &logger,
        const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    DEKPool dek_pool(session);
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
    AESCrypter aes_crypter;
    std::string master_key = assemble_master_key();
    aes_crypter.set_master_key(master_key);
    auto data_keys = Yb::query<Domain::DataKey>(session)
            .filter_by(Domain::DataKey::c.counter < 10)
            .all();
    for (auto &data_key : data_keys) {
        ElementTree::ElementPtr dk = resp->sub_element("data_key");
        std::string dek_crypted = data_key.dek_crypted.value();
        std::string dek_decoded = decode_base64(dek_crypted);
        std::string dek_decrypted = aes_crypter.decrypt(dek_decoded);
        dk->sub_element("id", Yb::to_string(data_key.id.value()));
        dk->sub_element("dek", dek_decrypted);
        dk->sub_element("counter", Yb::to_string(data_key.counter.value()));
        dk->sub_element("start_ts", Yb::to_string(data_key.start_ts.value()));
        dk->sub_element("finish_ts", Yb::to_string(data_key.finish_ts.value()));
    }
    return resp;
}

ElementTree::ElementPtr dek(Session &session, ILogger &logger,
        const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    DEKPool dek_pool(session);
    DataKey dek = dek_pool.get_active_data_key();
    dek.counter = dek.counter + 1;
    session.commit();
    resp->sub_element("dek", dek.dek_crypted.value());
    resp->sub_element("counter", Yb::to_string(dek.counter.value()));
    return resp;
}

ElementTree::ElementPtr get_token(Session &session, ILogger &logger,
        const StringDict &params) {
    ElementTree::ElementPtr resp = mk_resp("success");
    CardData card_data;
    try {
        std::string mode = params["mode"];
        if(mode.compare("auto") == 0) 
            card_data = generate_random_card_data();
        else {
            card_data._chname = params["chname"];
            card_data._pan = params["pan"];
            card_data._masked_pan = params["pan"];
            card_data._expdate = params["expdate"];
            card_data._cvn = params["cvn"];
        }
    } catch(Yb::KeyError &err) {
    }
    //make norm check
    if(card_data._pan.compare("") == 0)
        return mk_resp("error");

    CardCrypter card_crypter(session);
    std::string token = card_crypter.get_token(card_data);

    ElementTree::ElementPtr cd = resp->sub_element("card_data");
    cd->sub_element("chname",     card_data._chname);
    cd->sub_element("pan",        card_data._pan);
    cd->sub_element("masked_pan", card_data._masked_pan);
    cd->sub_element("expdate",    card_data._expdate);
    cd->sub_element("cvn",        card_data._cvn);
    resp->sub_element("token",    token);
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

    CardCrypter card_crypter(session);
    CardData card_data = card_crypter.get_card(token);
    resp->sub_element("chname",     card_data._chname);
    resp->sub_element("pan",        card_data._pan);
    resp->sub_element("masked_pan", card_data._masked_pan);
    resp->sub_element("expdate",    card_data._expdate);
    resp->sub_element("cvn",        card_data._cvn);
    return resp;
}

int main(int argc, char *argv[])
{
    AppSettings app_settings;
    app_settings.fill_tree();
    string log_name = "card_proxy.log";
    string db_name = "card_proxy_db";
    string error_content_type = "text/xml";
    string error_body = mk_resp("internal_error")->serialize();
    string prefix = "/card_bind/";//app_settings.get_prefix(); //"/card_bind/";
    int port = app_settings.get_port();//9119;
    CardProxyHttpWrapper handlers[] = {
        WRAP(bind_card),
        WRAP(dek_status),
        WRAP(dek_get),
        WRAP(dek_list),
        WRAP(dek),
        WRAP(get_token),
        WRAP(get_card),
    };
    int n_handlers = sizeof(handlers)/sizeof(handlers[0]);
    return run_server_app(log_name, db_name, port,
            handlers, n_handlers, error_content_type, error_body, prefix);
}

// vim:ts=4:sts=4:sw=4:et:
