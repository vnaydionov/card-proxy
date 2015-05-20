#ifndef CARDPROXY_H_
#define CARDPROXY_H_
#include "domain/Card.h"

class CardProxy {
public:
	CardProxy(const std::string &key);
	virtual ~CardProxy();

	std::string get_token(const CardData &card_data);
	Card get_data(const std::string &token);

private:
	unsigned char* find(const CardData &card_data);
	std::string _key;
};

#endif /* CARDPROXY_H_ */
