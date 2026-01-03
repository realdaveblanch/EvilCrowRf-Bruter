#pragma once
#include "protocol.h"

class protocol_dooya : public c_rf_protocol {
public:
    protocol_dooya() {
        // T=350us. Usado en motores de persianas y toldos.
        transposition_table['0'] = {350, -700};
        transposition_table['1'] = {700, -350};
        pilot_period = {350, -7000};
        stop_bit = {};
    }
};
