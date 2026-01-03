#pragma once
#include "protocol.h"

class protocol_intertechno_v3 : public c_rf_protocol {
public:
    protocol_intertechno_v3() {
        // Protocolo PWM específico. T=250us. 32 bits totales.
        transposition_table['0'] = {250, -250, 250, -1250};
        transposition_table['1'] = {250, -1250, 250, -250};
        pilot_period = {250, -2500};
        stop_bit = {250, -10000};
    }
};
