#pragma once
#include "protocol.h"

class protocol_unilarm : public c_rf_protocol {
public:
    protocol_unilarm() {
        // T=350us.
        transposition_table['0'] = {350, -1050, 350, -1050};
        transposition_table['1'] = {1050, -350, 1050, -350};
        pilot_period = {350, -10850};
        stop_bit = {};
    }
};
