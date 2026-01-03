#pragma once
#include "protocol.h"

class protocol_tedsen : public c_rf_protocol {
public:
    protocol_tedsen() {
        // T=600us. Muy robusto.
        transposition_table['0'] = {600, -1200};
        transposition_table['1'] = {1200, -600};
        pilot_period = {-15000, 600};
        stop_bit = {};
    }
};
