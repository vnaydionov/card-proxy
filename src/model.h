#ifndef _CARD_PROXY__MODEL_H_
#define _CARD_PROXY__MODEL_H_

#include "orm/domain_object.h"
#include "orm/domain_factory.h"
#include "orm/schema_decl.h"

class DataKey;

class Card;

class IncomingRequest: public Yb::DomainObject {

YB_DECLARE(IncomingRequest, "T_INCOMING_REQUEST", "S_INCOMING_REQUEST", "incoming_request",
    YB_COL_PK(id, "ID")
    YB_COL_DATA(ts, "TS", DATETIME)
    YB_COL_FK(card_id, "CARD_ID", "T_CARD", "ID")
    YB_COL_FK(dek_id, "DEK_ID", "T_DEK", "ID")
    YB_COL_STR(cvn, "CVN_CRYPT", 25)
    YB_REL_ONE(Card, card, IncomingRequest, incoming_requests,
               Yb::Relation::Restrict, "CARD_ID", 1, "")
    YB_REL_ONE(DataKey, data_key, IncomingRequest, incoming_requests,
               Yb::Relation::Restrict, "DEK_ID", 1, "")
    YB_COL_END)

};

class Card: public Yb::DomainObject {

YB_DECLARE(Order, "T_CARD", "S_CARD", "card",
    YB_COL_PK(id, "ID")
    YB_COL_DATA(ts, "TS", DATETIME)
    YB_COL_STR(token, "TOKEN", 32)
    YB_COL_FK(dek_id, "DEK_ID", "T_DEK", "ID")
    YB_COL_STR(pan_crypt, "PAN_CRYPT", 25)
    YB_COL_STR(pan_mask, "PAN_MASK", 25)
    YB_COL_STR(card_holder, "CARD_HOLDER", 100)
    YB_COL_DATA(expire_dt, "EXPIRE_DT", DATETIME)
    YB_COL_DATA(recur, "RECUR", INTEGER)
    YB_REL_ONE(DataKey, data_key, Card, cards,
               Yb::Relation::Restrict, "DEK_ID", 1, "")
    YB_COL_END)

};

class DataKey: public Yb::DomainObject {

YB_DECLARE(DataKey, "T_DEK", "S_DEK", "data_key",
    YB_COL_PK(id, "ID")
    YB_COL_DATA(ts, "TS", DATETIME)
    YB_COL_STR(dek_crypt, "DEK_CRYPT", 45)
    YB_COL_DATA(recur, "ACTIVE", INTEGER)
    YB_REL_ONE(DataKey, data_key, Card, cards,
               Yb::Relation::Restrict, "DEK_ID", 1, "")
    YB_REL_ONE(DataKey, data_key, IncomingRequest, incoming_requests,
               Yb::Relation::Restrict, "DEK_ID", 1, "")
    YB_COL_END)

};

class Config: public Yb::DomainObject {

YB_DECLARE(Payment, "T_CONFIG", "S_DEK", "data_key",
    YB_COL(ts, "TS", DATETIME)
    YB_COL_STR(dek_crypt, "DEK_CRYPT", 45)
    YB_COL_DATA(recur, "ACTIVE", INTEGER)
    YB_REL_ONE(Card, card, IncomingRequest, incoming_requests,
               Yb::Relation::Restrict, "CARD_ID", 1, "")
    YB_REL_ONE(DataKey, data_key, IncomingRequest, incoming_requests,
               Yb::Relation::Restrict, "DEK_ID", 1, "")
    YB_COL_END)

};

#endif // _CARD_PROXY__MODEL_H_
// vim:ts=4:sts=4:sw=4:et:
