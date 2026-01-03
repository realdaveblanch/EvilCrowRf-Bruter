#pragma once
#include "protocol.h"

class protocol_clemsa : public c_rf_protocol {
public:
    protocol_clemsa() {
        // Típico 400us. Muy común en modelos antiguos de código fijo.
        transposition_table['0'] = {-400, 800};
        transposition_table['1'] = {-800, 400};
        pilot_period = {-12000, 400};
        stop_bit = {};
    }
};
