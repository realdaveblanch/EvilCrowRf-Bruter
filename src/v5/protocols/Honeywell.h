#pragma once
#include "protocol.h"

class protocol_honeywell : public c_rf_protocol {
public:
    protocol_honeywell() {
        // T=300us.
        transposition_table['0'] = {300, -600};
        transposition_table['1'] = {600, -300};
        pilot_period = {300, -9000};
        stop_bit = {};
    }
};
