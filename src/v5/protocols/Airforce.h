#pragma once
#include "protocol.h"

class protocol_airforce : public c_rf_protocol {
public:
    protocol_airforce() {
        // T=350us. Muy similar al Princeton pero con pilot diferente.
        transposition_table['0'] = {350, -1050, 350, -1050};
        transposition_table['1'] = {1050, -350, 1050, -350};
        pilot_period = {350, -10850};
        stop_bit = {};
    }
};
