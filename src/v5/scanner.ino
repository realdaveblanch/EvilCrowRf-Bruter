#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "config.h"

extern void dualPrintf(const char* format, ...);
extern void dualPrintln(String s);
extern void setFrequencyCorrected(float target_mhz);
extern int checkPause();
extern char waitKey();

// Simple RSSI Scanner to find active frequencies
void runScanner() {
    dualPrintln("\n=== RF SIGNAL SCANNER (RSSI) ===");
    dualPrintln("Scanning 315, 433, 868 MHz...");
    dualPrintln("Press 'C' to stop.");

    float freqs[] = {315.00, 433.92, 868.35, 300.00, 390.00};
    int max_rssi = -120;
    float best_freq = 0;

    while(true) {
        if (checkPause() == 2) return;

        for (float f : freqs) {
            ELECHOUSE_cc1101.setMHZ(f - CC1101_FREQ_OFFSET); // Manual set to avoid print spam
            ELECHOUSE_cc1101.SetRx();
            delay(20); // Settling time
            
            int rssi = ELECHOUSE_cc1101.getRssi();
            
            // Simple visualizer
            if (rssi > -80) { // Threshold
                 dualPrintf("[!] SIGNAL FOUND: %.2f MHz | RSSI: %d dBm\n", f, rssi);
                 // ASCII Bar
                 dualPrint("    Level: ");
                 int bars = (rssi + 100) / 2;
                 if(bars<0) bars=0;
                 for(int i=0; i<bars; i++) dualPrint("#");
                 dualPrintln("");
            }
        }
        delay(50);
    }
}
