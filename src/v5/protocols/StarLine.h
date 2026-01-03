#pragma once
#include "protocol.h"

class protocol_starline : public c_rf_protocol {
public:
    protocol_starline() {
        // T=500us típico
        transposition_table['0'] = {500, -1000};
        transposition_table['1'] = {1000, -500};
        pilot_period = {-10000, 500};
        stop_bit = {};
    }
};
