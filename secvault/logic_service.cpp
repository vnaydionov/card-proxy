// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#include "logic_service.h"
#include "app_class.h"
#include "tokenizer.h"
#include "servant_utils.h"

namespace LogicService {

Yb::ElementTree::ElementPtr ping(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    session.engine()->exec_select("select 1 from dual", Yb::Values());
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
    return resp;
}

Yb::ElementTree::ElementPtr check_kek(
        Yb::Session &session, Yb::ILogger &logger,
        const Yb::StringDict &params)
{
    Yb::ElementTree::ElementPtr resp = mk_resp("success");
#ifdef TOKENIZER_CONFIG_SINGLETON
    TokenizerConfig &tcfg(theTokenizerConfig::instance().refresh());
#else
    TokenizerConfig tcfg;
#endif
    std::string ver = params.get("kek_version", "");
    int kek_version = -1;
    if (ver.empty())
        kek_version = tcfg.get_active_master_key_version();
    else
        kek_version = boost::lexical_cast<int>(ver);
    resp->sub_element("check_kek",
            tcfg.is_kek_valid(kek_version)? "true": "false");
    return resp;
}

} // LogicService

// vim:ts=4:sts=4:sw=4:et:
