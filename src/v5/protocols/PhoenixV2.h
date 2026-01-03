#pragma once
#include "protocol.h"

class protocol_phoenix_v2 : public c_rf_protocol {
public:
    protocol_phoenix_v2() {
        // T=500us. Muy usado en mandos de garaje europeos.
        transposition_table['0'] = {-500, 1000};
        transposition_table['1'] = {-1000, 500};
        pilot_period = {-15000, 500};
        stop_bit = {};
    }
};
