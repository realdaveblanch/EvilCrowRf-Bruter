#pragma once

// --- SECURITY & OPSEC ---
#define BT_DEVICE_NAME "EvilCrowRf Bruter - Open Source Edition"  // Discreto

// --- RF HARDWARE CONFIG ---
#define CC1101_FREQ_OFFSET 0.052  // Ajustar con SDR si es posible

// --- PIN DEFINITIONS (ESP32 Generic/Pico) ---
#define PIN_RF_CS   27
#define PIN_RF_GDO0 26
#define PIN_RF_TX   25
#define PIN_RF_SCK  14
#define PIN_RF_MISO 12
#define PIN_RF_MOSI 13
