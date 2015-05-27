#ifndef CARDDATA_H_
#define CARDDATA_H_
#include "helpers.h"

class CardData { 
public:
    CardData();
    CardData(const std::string &chname, const std::string &pan,
            const std::string &masked_pan, const std::string &expdate,
            const std::string &cvn);
    virtual ~CardData();

    std::string _chname, _pan, _masked_pan, _expdate, _cvn;
};
#endif
