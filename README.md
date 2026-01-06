# EvilCrowRf Bruter - Open Source Edition
# EvilCrowRf Bruter - Open Source Edition

![Status](https://img.shields.io/badge/Status-Stable-green.svg) ![Platform](https://img.shields.io/badge/Platform-ESP32%20%7C%20CC1101-blue.svg) ![Protocols](https://img.shields.io/badge/Protocols-34%20Supported-orange.svg) ![License](https://img.shields.io/badge/License-MIT-lightgrey.svg) ![Documentation](https://img.shields.io/badge/Documentation-Encyclopedia-purple.svg)

**Versión:** 1.0 
**Fecha de Revisión:** 6 de Enero de 2026  
**Autor Principal:** David Rodríguez Pérez  
**Arquitectura:** EvilCrowRfV2 - Espressif ESP32 (PICO D4) + Texas Instruments CC1101  


---

# 📚 Índice de Contenidos / Table of Contents

1.  [🇪🇸 ESPAÑOL: Documentación de Ingeniería](#-español-documentación-de-ingeniería)
    *   [1. Introducción y Alcance del Proyecto](#1-introducción-y-alcance-del-proyecto)
    *   [2. Fundamentos Teóricos de Radiofrecuencia](#2-fundamentos-teóricos-de-radiofrecuencia)
    *   [3. Arquitectura del Hardware](#3-arquitectura-del-hardware)
    *   [4. Enciclopedia Detallada de Protocolos (Análisis PHY)](#4-enciclopedia-detallada-de-protocolos-análisis-phy)
    *   [5. Análisis Matemático de Vectores de Ataque](#5-análisis-matemático-de-vectores-de-ataque)
    *   [6. Manual de Operación y Comandos](#6-manual-de-operación-y-comandos)
    *   [7. Solución de Problemas (Troubleshooting)](#7-solución-de-problemas-troubleshooting)
2.  [🇺🇸 ENGLISH: Technical Reference](#-english-technical-reference)

---

# 🇪🇸 ESPAÑOL: Documentación de Ingeniería

## 1. Introducción y Alcance del Proyecto

El proyecto **EvilCrowRf Bruter - Open Source Edition** no es simplemente una herramienta de hacking; es un compendio de investigación sobre la inseguridad inherente en los sistemas de control de acceso inalámbrico que dominaron el mercado entre 1990 y 2015, y que aún persisten en millones de instalaciones.

Diseñado y programado por **David Rodríguez Pérez**, este sistema utiliza un microcontrolador de doble núcleo (ESP32) para sintetizar señales de radiofrecuencia complejas en tiempo real, inyectándolas en el espectro electromagnético a través del transceptor CC1101. El objetivo es demostrar empíricamente cómo la entropía limitada de los códigos fijos (Fixed Code) y los sistemas Tristate (ternarios) hace trivial su compromiso mediante ataques de fuerza bruta (barrido exhaustivo del espacio de claves).

### 1.1 Objetivos Técnicos
*   **Universalidad:** Capacidad de modular señales en múltiples frecuencias (300 MHz - 928 MHz) sin cambios de hardware.
*   **Precisión:** Generación de pulsos con tolerancias de $\pm 10 \mu s$ para engañar a los filtros de los receptores.


---

## 2. Fundamentos Teóricos de Radiofrecuencia

Para entender el funcionamiento de este firmware, es necesario comprender la física subyacente de la transmisión de datos en bandas ISM (Industrial, Scientific, Medical).

### 2.1 Modulación ASK / OOK
El 99% de los mandos de garaje y alarmas básicas utilizan **ASK (Amplitude Shift Keying)** en su variante más simple: **OOK (On-Off Keying)**.
*   **Estado Lógico 1 (Mark):** El transmisor enciende la portadora (Carrier ON). El consumo de corriente es máximo.
*   **Estado Lógico 0 (Space):** El transmisor apaga la portadora (Carrier OFF). El consumo es casi nulo.

A diferencia de FSK (Frequency Shift Keying) donde se cambia la frecuencia, OOK es muy susceptible al ruido, pero extremadamente barato de implementar. Esto permite que nuestro ataque sea efectivo simplemente "gritando" más fuerte (mayor RSSI) que el mando original.

### 2.2 Codificación de Línea (Layer 1)
La información binaria (0s y 1s) debe traducirse a tiempos de encendido y apagado. El firmware soporta los tres esquemas principales:

#### A. PWM (Pulse Width Modulation)
El valor del bit se codifica en la duración del estado alto (High) frente al estado bajo (Low).
*   **Bit 0:** Pulso Corto (H) + Espacio Largo (L). Relación típica 1:2 o 1:3.
*   **Bit 1:** Pulso Largo (H) + Espacio Corto (L). Relación inversa.
*   *Usado por:* PT2262, EV1527, HT12E.

#### B. Manchester (Bi-Phase)
El valor se codifica en la **transición** a mitad del periodo de bit.
*   **Bit 0:** Transición de Bajo a Alto (o viceversa, dependiendo de la norma IEEE o Thomas).
*   **Bit 1:** Transición opuesta.
*   *Usado por:* CAME, NiceFlo. Es más robusto al ruido y permite recuperar el reloj (Clock Recovery) más fácilmente.

#### C. Tristate (Lógica Ternaria)
Utilizado para aumentar la entropía sin aumentar la longitud de la trama en chips antiguos con pocos pines. Cada "trit" puede ser:
*   **0:** Conectado a GND.
*   **1:** Conectado a VCC.
*   **F (Float):** Pin desconectado (Alta impedancia).
Esto convierte un sistema de 12 pines en un espacio de claves de $3^{12} = 531,441$ combinaciones, frente a las $2^{12} = 4096$ de un sistema binario.

---

## 3. Arquitectura del Hardware

El sistema se basa en la interconexión SPI de alta velocidad.

### 3.1 Diagrama de Conexiones (Pinout)

| Señal | ESP32 GPIO | CC1101 Pin | Descripción Funcional |
|:------|:----------:|:----------:|:----------------------|
| **VCC** | 3V3 | VCC | Alimentación principal (3.0 - 3.6V). |
| **GND** | GND | GND | Tierra común. |
| **SCK** | GPIO 14 | SCK | Reloj del bus SPI. |
| **MISO** | GPIO 12 | MISO | Datos del CC1101 al ESP32. |
| **MOSI** | GPIO 13 | MOSI | Comandos del ESP32 al CC1101. |
| **CSN** | GPIO 27 | CSN | Chip Select (Activo bajo). |
| **GDO0** | GPIO 26 | GDO0 | Salida de modulación (TX asíncrono). |
| **GDO2** | N/C | GDO2 | Opcional para interrupciones RX. |

### 3.2 Teoría de Antenas
Para un rendimiento óptimo, la longitud de la antena debe ser $\lambda / 4$ (cuarto de onda).
$$ \lambda = \frac{c}{f} $$
*   **Para 433.92 MHz:** $\lambda \approx 69 \text{ cm}$. Antena óptima $\approx 17.3 \text{ cm}$.
*   **Para 868.35 MHz:** $\lambda \approx 34.5 \text{ cm}$. Antena óptima $\approx 8.6 \text{ cm}$.

Usar una antena de longitud incorrecta crea una desadaptación de impedancia (VSWR alto), reflejando potencia hacia el chip y reduciendo el alcance drásticamente.

---

## 4. Enciclopedia Detallada de Protocolos (Análisis PHY)

A continuación, se detalla la estructura física de los protocolos implementados, basándonos en la lectura del código fuente (`protocols/*.h`).

### 4.1 Grupo 1: Europa Estándar (433.92 MHz)

#### **CAME (12 & 24 bits)**
El rey de los automatismos en Europa. Utiliza una codificación Manchester peculiar donde el "start bit" es esencialmente un pulso piloto.
*   **Timing ($T_{bit}$):** 320 $\mu s$.
*   **Estructura Bit '0':** `High (320µs) - Low (640µs)` (Lógica invertida en transmisión).
*   **Estructura Bit '1':** `High (640µs) - Low (320µs)`.
*   **Diagrama:**
    ```text
          +---+       +-------+
    Pilot |   |_______|       |___ ...
           320   11520   640
    ```

#### **Nice FLO (Fixed Code)**
No confundir con "Nice Flor-S" (Rolling Code). Este es el sistema antiguo de mandos azules grandes con dip-switches internos.
*   **Timing:** 700 $\mu s$ (Muy lento, lo que le da gran alcance).
*   **Estructura:** Manchester simple.
*   **Piloto:** 25ms de señal baja antes de empezar.

#### **Clemsa (Mastercode)**
Protocolo español muy extendido.
*   **Timing:** 400 $\mu s$.
*   **Modulación:** PWM.
*   **Bit 0:** `Low (400µs) - High (800µs)`.
*   **Bit 1:** `Low (800µs) - High (400µs)`.
### 4.2 Grupo 2: Protocolos Tristate (PT2262)

Estos protocolos son los más complejos de atacar debido a su estructura ternaria.

#### **Princeton (PT2262)**
El estándar de facto para clones chinos y sistemas baratos.
*   **Timing Base ($\alpha$):** 350 $\mu s$ (puede variar de 150 a 500 según la resistencia de oscilación).
*   **Codificación de Bits (cada bit lógico son 2 ciclos físicos):
    *   `0`: High($\alpha$)-Low(3$\alpha$)-High($\alpha$)-Low(3$\alpha$).
    *   `1`: High(3$\alpha$)-Low($\alpha$)-High(3$\alpha$)-Low($\alpha$).
    *   `F`: High($\alpha$)-Low(3$\alpha$)-High(3$\alpha$)-Low($\alpha$).
*   **Sync:** Un pulso Low masivo de 31$\alpha$ al final de la palabra.

#### **SMC5326**
Muy similar al PT2262 pero popular en Malasia y sistemas de alarma antiguos.
*   **Frecuencias:** Común en 315 MHz y 433.42 MHz (desviado del estándar .92).
*   **Timing:** 320 $\mu s$.
### 4.3 Grupo 3: Estados Unidos (Legacy)

#### **Chamberlain / Liftmaster (Security+ 1.0)**
Antes de Security+ 2.0 (que es encriptado), existía esta versión susceptible.
*   **Frecuencias:** Salta entre 300, 315 y 390 MHz.
*   **Bits:** Tramas variables de 8, 9 o 12 bits.
*   **Peculiaridad:** El pulso de sincronización va al **final** de la trama, no al principio.

#### **Linear MegaCode**
Utilizado en sistemas de control de acceso comercial.
*   **Frecuencia:** 318 MHz (Banda estrecha).
*   **Bits:** 24 bits de código fijo (aunque el nombre suene a avanzado).
*   **Timing:** 500 $\mu s$.

#### **Firefly (Dip Switch)**
Sistemas muy antiguos (años 80-90) de 300 MHz.
*   **Timing:** 400 $\mu s$.
*   **Seguridad:** Nula. 10 Dip Switches = 1024 combinaciones. Se abre en < 3 segundos.
### 4.4 Grupo 4: Alta Seguridad (868 MHz)

El salto a 868 MHz buscaba evitar interferencias, pero muchos fabricantes portaron sus protocolos inseguros a la nueva frecuencia.

#### **Hormann (HSM4)**
Mandos de botones azules.
*   **Frecuencia:** 868.35 MHz.
*   **Modulación:** FSK/OOK Híbrido en algunos modelos, pero el ataque OOK funciona.
*   **Timing:** 500 $\mu s$.

#### **Marantec (Digital)**
Sistemas alemanes de alta precisión.
*   **Frecuencia:** 868.35 MHz.
*   **Timing:** 600 $\mu s$.
*   **Tolerancia:** Muy baja. Si el timing varía más del 5%, el receptor rechaza la trama. El ESP32 es crucial aquí por su estabilidad de reloj frente a un Arduino Uno.

---

## 5. Análisis Matemático de Vectores de Ataque

La ingeniería de seguridad requiere cuantificar el tiempo necesario para comprometer un sistema.

### 5.1 Ecuación General de Fuerza Bruta
El tiempo total ($T_{total}$) viene dado por:

$$ T_{total} = \sum_{i=0}^{N-1} (T_{frame} + T_{inter\_frame\_gap}) \times R $$

Donde:
*   $N$: Tamaño del espacio de claves ($Base^{Bits}$). 
*   $T_{frame}$: Duración de una trama individual.
*   $T_{inter\_frame\_gap}$: Tiempo de silencio obligatorio entre tramas (usualmente $>10ms$).
*   $R$: Número de repeticiones por código (Redundancia).

### 5.2 Escenarios de Ataque Real

#### A. Ataque a un sistema binario de 10 bits (Ej. Linear 300MHz)
*   **Espacio ($N$):** $2^{10} = 1024$ códigos.
*   **Duración Trama:** ~20ms.
*   **Repeticiones:** 3.
*   **Cálculo:** $1024 \times 0.020s \times 3 = 61.44$ segundos.
*   *Resultado:* Acceso garantizado en 1 minuto.

#### B. Ataque a un sistema Tristate de 12 trits (Ej. Princeton Full)
*   **Espacio ($N$):** $3^{12} = 531,441$ códigos.
*   **Duración Trama:** ~45ms.
*   **Repeticiones:** 3.
*   **Cálculo:** $531,441 \times 0.045s \times 3 \approx 71,744$ segundos $\approx 19.9$ horas.
*   *Estrategia:* Inviable para un ataque casual. Se deben usar diccionarios de claves comunes (ej. todos los DIPs en OFF, o solo 1 en ON).

---

## 6. Manual de Operación y Comandos

### 6.1 Preparación del Entorno
1.  **Hardware:** Conectar el módulo CC1101 al ESP32 según el pinout de la sección 3.1.
2.  **Software:** Compilar el proyecto usando PlatformIO o Arduino IDE 2.x.
    *   *Nota:* Asegúrese de que la librería `ELECHOUSE_CC1101` está correctamente importada en `src`.

3. **Uso Bluetooth** 
    Descargar cualquier aplicación que permita puente serial por Bluetooth:

    https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal&hl=es&pli=1

    Y mismo uso que por serial USB. (Puede ser necesario enviar algun caracter para que se renderice el menú)

### 6.2 Interfaz de Usuario (CLI)
Al iniciar, abra el monitor serie a **115200 baudios**. Verá el siguiente menú:

```text
==========================================
EvilCrowRf Bruter - Open Source Edition      
==========================================
1. Europe Mandos (CAME, Nice, Clemsa...)
2. Tristate / Chinos (Princeton, SMC...)
3. Domotica / Persianas (Dooya, Nero...)
4. USA Garage (Chamberlain, Liftmaster)
5. USA Old (Linear, Firefly, MegaCode)
6. Europe 868 MHz (Hormann, Marantec)
7. Intertechno V3 (32 bits)
8. Alarmas 24 bits (EV1527)
9. Otros (StarLine, Tedsen, Airforce)
R. Ajustar REPETICIONES (Actual: 4)
```

### 6.3 Procedimiento de Ataque
1.  Identifique visualmente el receptor o el mando de la víctima para estimar la frecuencia y marca.
    *   *Tip:* Antena larga suele ser 433 MHz, antena corta 868 MHz.
2.  Seleccione la categoría correspondiente en el menú.
3.  El sistema comenzará a transmitir. El LED de TX del CC1101 (si tiene) parpadeará frenéticamente.
4.  La consola mostrará el progreso: `[CAME 12b] 15.4%`.
5.  Si la puerta se abre, pulse cualquier tecla para detener el ataque y anote el porcentaje aproximado para reducir el rango en futuros intentos.

---

## 7. Solución de Problemas (Troubleshooting)

### El sistema no detecta el CC1101
*   **Síntoma:** Mensaje "CC1101 no detectado" en el arranque.
*   **Solución:** Verifique el cableado. El CC1101 es muy sensible a conexiones flojas en el bus SPI. Revise que `CSN` vaya al GPIO 27.

### Rango muy corto (< 1 metro)
*   **Causa 1:** Antena incorrecta. Verifique la longitud (17cm para 433).
*   **Causa 2:** El CC1101 chino barato a veces viene con el circuito de adaptación (Balun) mal diseñado para 433 MHz (usando componentes de 868 o viceversa).

### La puerta no se abre tras el 100%
*   **Causa 1:** El sistema usa Rolling Code (HCS301, Keeloq). Este ataque NO funciona contra Rolling Code.
*   **Causa 2:** La frecuencia está ligeramente desplazada. Intente editar `*.ino` y ajustar `FREQ_OFFSET` (ej. de 0.052 a 0.030).
*   **Causa 3:** El receptor requiere más repeticiones. Use la opción 'R' del menú y suba a 6 o 8 repeticiones.

---

# 🇺🇸 ENGLISH: Technical Reference

## 1. Introduction and Scope

**EvilCrowRf Bruter - Open Source Edition** is a comprehensive research project into the vulnerabilities of ISM band wireless access control systems. Developed by **David Rodríguez Pérez**, this ESP32-based firmware acts as a high-speed signal synthesizer capable of exhausting the key space of Fixed Code protocols.

The tool is strictly designed for **educational purposes** and security auditing of authorized equipment.

## 2. RF Theory & Fundamentals

### 2.1 Modulation Techniques
El sistema primarily exploits **OOK (On-Off Keying)**, the simplest form of Amplitude Shift Keying.
*   **Simplicity:** OOK transmitters are ubiquitous in low-cost hardware (garage openers, doorbells).
*   **Vulnerability:** Lack of encryption and small key spaces (12-24 bits) make them ideal targets for brute-force attacks.

### 2.2 Supported Line Codes
*   **PWM:** Uses pulse width ratios (e.g., 1:3 vs 3:1) to define logic states.
*   **Manchester:** Uses transition edges (Rising/Falling) for clock recovery and data integrity.
*   **Tristate:** Exploits the 3-state pins of older ICs (High, Low, Floating) to increase key density.

## 3. Supported Protocols Specifications

### European Protocols (433.92 MHz)
| Protocol | Modulation | Bit Length ($\mu s$) | Structure |
|:---|:---:|:---:|:---:|
| **CAME** | Manchester | 320 | Inverted logic. Pilot pulse required. |
| **NiceFlo** | Manchester | 700 | Slow baud rate for range. |
| **Clemsa** | PWM | 400 | Spanish market standard. |
| **Prastel** | PWM | 400 | Common in France/Italy. |

### US Legacy Protocols (300-390 MHz)
| Protocol | Freq (MHz) | Notes |
|:---|:---:|:---:|
| **Chamberlain** | 300/315/390 | Security+ 1.0 (Fixed parts). |
| **Linear** | 300/310 | Delta-3 DIP switch systems. |
| **MegaCode** | 318 | 24-bit fixed code on narrow band. |

### Asian / Generic Protocols
*   **PT2262 (Princeton):** The basis for 80% of generic remote controls. Supports Tristate logic.
*   **EV1527:** Learning code OTP chips. Vulnerable to replay and jamming/brute-force.

## 4. Operational Manual

1.  **Wiring:** Connect CC1101 to ESP32 via SPI.
2.  **Frequency Correction:** The code includes a software offset to correct cheap crystal oscillators.
3.  **Execution:**
    *   Connect via Serial (115200 baud).
    *   Navigate the menu using number keys.
    *   The system will automatically configure the CC1101 registers (Frequency, Deviation, Modulation) for the selected protocol and begin the attack sequence.

4.  **Bluetooth Usage**
    Download any application that allows a serial bridge over Bluetooth:

    https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal&hl=es&pli=1

    Use it in the same way as with USB serial. (It may be necessary to send some character in order for the menu to be rendered)

## 5. Disclaimer

**DISCLAIMER:** This software is for **EDUCATIONAL USE ONLY**.
The author, **David Rodríguez Pérez**, is not responsible for any illegal use of this tool. Attempting to open gates, barriers, or doors that do not belong to you is a crime in most jurisdictions. Use this tool only on equipment you own or have explicit permission to audit.

---
*Generated: January 6rd, 2026*  
*Project: EvilCrowRf Bruter - Open Source Edition v1.0*