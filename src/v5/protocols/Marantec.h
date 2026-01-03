#pragma once
#include "protocol.h"

class protocol_marantec : public c_rf_protocol {
public:
    protocol_marantec() {
        // T=600us. Protocolo muy preciso con preámbulo largo.
        transposition_table['0'] = {600, -1200};
        transposition_table['1'] = {1200, -600};
        pilot_period = {600, -15000};
        stop_bit = {600, -25000};
    }
};
