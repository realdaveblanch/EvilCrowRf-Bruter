#pragma once
#include "protocol.h"

class protocol_linear_megacode : public c_rf_protocol {
public:
    protocol_linear_megacode() {
        // T=500us. Protocolo de 24 bits.
        transposition_table['0'] = {500, -1000};
        transposition_table['1'] = {1000, -500};
        pilot_period = {500, -15000};
        stop_bit = {};
    }
};
