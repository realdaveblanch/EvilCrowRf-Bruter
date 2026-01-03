#pragma once
#include "protocol.h"

class protocol_liftmaster : public c_rf_protocol {
public:
    protocol_liftmaster() {
        // Tiempos típicos: 400us base
        transposition_table['0'] = {400, -800};
        transposition_table['1'] = {800, -400};
        pilot_period = {-15000, 400};
        stop_bit = {};
    }
};