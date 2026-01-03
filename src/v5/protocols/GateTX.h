#pragma once
#include "protocol.h"

class protocol_gate_tx : public c_rf_protocol {
public:
    protocol_gate_tx() {
        // T=350us.
        transposition_table['0'] = {-350, 700};
        transposition_table['1'] = {-700, 350};
        pilot_period = {-11000, 350};
        stop_bit = {};
    }
};
