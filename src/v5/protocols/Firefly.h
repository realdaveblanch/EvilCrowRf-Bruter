#pragma once
#include "protocol.h"

class protocol_firefly : public c_rf_protocol {
public:
    protocol_firefly() {
        // T=400us. Usado en 300MHz.
        transposition_table['0'] = {400, -800};
        transposition_table['1'] = {800, -400};
        pilot_period = {400, -12000};
        stop_bit = {};
    }
};
