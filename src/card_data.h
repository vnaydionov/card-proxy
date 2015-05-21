#ifndef CARDDATA_H_
#define CARDDATA_H_
#include "helpers.h"

class CardData { 
public:
    CardData();
    CardData(std::string chname, std::string pan, std::string expdate, std::string cvn);
    virtual ~CardData();

   std::string chname, pan, expdate, cvn;
};
#endif
