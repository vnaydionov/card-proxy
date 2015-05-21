#ifndef CARDPROXY_H_
#define CARDPROXY_H_
#include "domain/Card.h"
#include "card_data.h"

class CardProxy {
public:
	CardProxy(const std::string &key);
	virtual ~CardProxy();

	std::string get_card_token(Yb::Session &session, const CardData &card_data);
	CardData get_card_data(Yb::Session &session, const std::string &token);

private:
	unsigned char* find_card(const CardData &card);
	std::string _key;
};

#endif /* CARDPROXY_H_ */
