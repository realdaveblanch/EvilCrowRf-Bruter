#pragma once
#include "protocol.h"

class protocol_ev1527 : public c_rf_protocol {
public:
    protocol_ev1527() {
        // T=320us típico para sensores y mandos EV1527
        transposition_table['0'] = {320, -960};
        transposition_table['1'] = {960, -320};
        pilot_period = {320, -9920};
        stop_bit = {};
    }
};
