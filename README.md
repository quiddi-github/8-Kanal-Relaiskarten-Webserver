# 🌐 Arduino Mega 2560 – Ethernet Relais Steuerung (W5500)

Webbasierte 8-Kanal-Relaissteuerung mit **Arduino Mega 2560** und **W5500 Ethernet Modul**.  
Steuerung per Webbrowser im lokalen Netzwerk inklusive Impulsfunktionen für Power-Buttons.

---

## 📌 Features

- ✅ 8 Relais (HIGH-aktiv)
- ✅ 8 Taster mit Entprellung (INPUT_PULLUP)
- ✅ Webserver auf Port 80
- ✅ Auto-Refresh der Statusseite (5 Sekunden)
- ✅ Impulssteuerung:
  - Relais 1 → 2 Sekunden
  - Relais 2 → 1 oder 10 Sekunden
- ✅ Feste IP-Adresse
- ✅ Kein JavaScript notwendig

---

## 🧰 Hardware

- Arduino Mega 2560
- W5500 Ethernet Modul
- 8-Kanal Relaismodul (HIGH-aktiv)
- 8 Taster (gegen GND geschaltet)
- LAN Netzwerk

---

## 🔌 Verdrahtung

### SPI-Verbindung (Mega 2560 ↔ W5500)

| W5500 | Arduino Mega 2560 |
|-------|-------------------|
| MOSI  | Pin 51 |
| MISO  | Pin 50 |
| SCK   | Pin 52 |
| CS    | Pin 53 |
| VCC   | 5V oder 3.3V* |
| GND   | GND |

\* Je nach verwendetem Modul!

> Hinweis: Pin 53 muss beim Mega als OUTPUT gesetzt sein, auch wenn ein anderer CS-Pin verwendet wird.

---

### Relais-Pins

| Relais | Arduino Pin |
|--------|------------|
| 1 | 22 |
| 2 | 23 |
| 3 | 24 |
| 4 | 25 |
| 5 | 26 |
| 6 | 27 |
| 7 | 28 |
| 8 | 29 |

---

### Taster-Pins (INPUT_PULLUP)

| Taster | Arduino Pin |
|--------|------------|
| 1 | 32 |
| 2 | 33 |
| 3 | 34 |
| 4 | 35 |
| 5 | 36 |
| 6 | 37 |
| 7 | 38 |
| 8 | 39 |

Taster werden gegen **GND** geschaltet.

---

## 🌍 Netzwerk-Konfiguration

```cpp
byte mac[] = { 0x1E, 0xAD, 0x2E, 0xEF, 0xFE, 0xEA };

IPAddress ip(192, 168, 0, 199);
IPAddress gateway(192, 168, 0, 253);
IPAddress subnet(255, 255, 255, 0);