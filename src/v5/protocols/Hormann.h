#pragma once
#include "protocol.h"

class protocol_hormann : public c_rf_protocol {
public:
    protocol_hormann() {
        // T=500us. Clásico mando gris con botones azules (868) o amarillos (433).
        transposition_table['0'] = {500, -500};
        transposition_table['1'] = {1000, -500};
        pilot_period = {500, -10000};
        stop_bit = {};
    }
};
