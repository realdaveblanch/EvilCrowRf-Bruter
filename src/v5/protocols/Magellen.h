#pragma once
#include "protocol.h"

class protocol_magellen : public c_rf_protocol {
public:
    protocol_magellen() {
        // T=400us.
        transposition_table['0'] = {400, -800};
        transposition_table['1'] = {800, -400};
        pilot_period = {400, -12000};
        stop_bit = {};
    }
};
