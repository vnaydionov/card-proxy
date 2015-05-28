#include "card_data.h"

CardData::CardData() {

}

CardData::CardData(const std::string &chname, const std::string &pan,
        const std::string &masked_pan, const std::string &expdate,
        const std::string &cvn)
        : _chname(chname), _pan(pan), _masked_pan(masked_pan)
        , _expdate(expdate), _cvn(cvn) {
}


