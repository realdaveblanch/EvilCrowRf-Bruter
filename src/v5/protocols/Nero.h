#pragma once
#include "protocol.h"

class protocol_nero : public c_rf_protocol {
public:
    protocol_nero() {
        // T=450us.
        transposition_table['0'] = {450, -900};
        transposition_table['1'] = {900, -450};
        pilot_period = {450, -13500};
        stop_bit = {};
    }
};
