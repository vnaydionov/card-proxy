#include "card_data.h"

CardData::CardData() {
}

CardData::CardData(std::string chname, std::string pan,
                   std::string expdate, std::string cvn) {
    this->chname = chname;
    this->pan = pan;
    this->expdate = expdate;
    this->cvn = cvn;
}
