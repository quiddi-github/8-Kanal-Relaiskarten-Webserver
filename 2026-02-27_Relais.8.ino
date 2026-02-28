#include <SPI.h>
#include <Ethernet.h>

/* =======================
   Hardware-Konfiguration
   ======================= */
#define W5500_CS 53

// Relais-Pins (HIGH-aktiv)
const byte relayPins[8]  = {22, 23, 24, 25, 26, 27, 28, 29};
// Taster-Pins (INPUT_PULLUP, gegen GND)
const byte buttonPins[8] = {32, 33, 34, 35, 36, 37, 38, 39};

/* ============================================================
   BESCHRIFTUNG
   ============================================================ */
const char* relayNames[8] = {
  "NAS Power-Button",     // Relais 1 (Impuls 2s)
  "Proxmox-Server",        // Relais 2 (Impuls 1s / 10s)
  "Telefon DECT Station",     // Relais 3
  "Relais 4", // Relais 4
  "Relais 5",
  "Relais 6",
  "Relais 7",
  "Relais 8"
};

/* =======================
   Netzwerk-Konfiguration
   ======================= */
byte mac[] = { 0x1E, 0xAD, 0x2E, 0xEF, 0xFE, 0xEA };
IPAddress ip(192, 168, 0, 199);
IPAddress gateway(192, 168, 0, 253);
IPAddress subnet(255, 255, 255, 0);

EthernetServer server(80);

/* =======================
   Statusvariablen
   ======================= */
bool relayState[8] = { false, false, false, false, false, false, false, false };
bool lastButtonState[8] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };
bool lastStableButtonState[8] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };

unsigned long relay1Timer = 0;
unsigned long relay1Duration = 2000; 

unsigned long relay2Timer = 0;
unsigned long relay2Duration = 1000; 

/* =======================
   Entprellungsvariablen
   ======================= */
unsigned long lastDebounceTime[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
const unsigned long DEBOUNCE_DELAY = 50;  

void updateRelays() {
  for (byte i = 0; i < 8; i++) {
    digitalWrite(relayPins[i], relayState[i] ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(9600);
  
  for (byte i = 0; i < 8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  for (byte i = 0; i < 8; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonState[i] = digitalRead(buttonPins[i]);
    lastStableButtonState[i] = lastButtonState[i];
  }

  pinMode(53, OUTPUT);
  Ethernet.init(W5500_CS);
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  server.begin();
  
  Serial.print("Server bereit unter: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // Auto-OFF Relais 1
  if (relayState[0] && (millis() - relay1Timer >= relay1Duration)) {
    relayState[0] = false;
    digitalWrite(relayPins[0], LOW);
  }

  // Auto-OFF Relais 2
  if (relayState[1] && (millis() - relay2Timer >= relay2Duration)) {
    relayState[1] = false;
    digitalWrite(relayPins[1], LOW);
  }

  // Taster abfragen
  for (byte i = 0; i < 8; i++) {
    bool currentReading = digitalRead(buttonPins[i]);
    if (currentReading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }
    
    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_DELAY) {
      if (currentReading != lastStableButtonState[i]) {
        lastStableButtonState[i] = currentReading;
        
        if (currentReading == LOW) { 
          if (i == 0) { 
            relayState[0] = true;
            relay1Duration = 2000;
            relay1Timer = millis();
          } 
          else if (i == 1) { 
            relayState[1] = true;
            relay2Duration = 1000;
            relay2Timer = millis();
          } 
          else { 
            relayState[i] = !relayState[i];
          }
          digitalWrite(relayPins[i], relayState[i] ? HIGH : LOW);
        }
      }
    }
    lastButtonState[i] = currentReading;
  }

  // Webserver
  EthernetClient client = server.available();
  if (!client) return;

  char request[100];
  int index = 0;
  while (client.connected() && index < 99) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') break;
      request[index++] = c;
    }
  }
  request[index] = '\0';

  bool actionRequested = false;

  // Befehle verarbeiten
  if (strstr(request, "GET /on1")) {
    relayState[0] = true; relay1Duration = 2000; relay1Timer = millis(); actionRequested = true;
  }
  else if (strstr(request, "GET /on2_short")) {
    relayState[1] = true; relay2Duration = 1000; relay2Timer = millis(); actionRequested = true;
  }
  else if (strstr(request, "GET /on2_long")) {
    relayState[1] = true; relay2Duration = 10000; relay2Timer = millis(); actionRequested = true;
  }
  else {
    for (byte i = 2; i < 8; i++) {
      char onCmd[12], offCmd[12];
      sprintf(onCmd, "GET /on%d", i + 1);
      sprintf(offCmd, "GET /off%d", i + 1);
      if (strstr(request, onCmd)) { relayState[i] = true; actionRequested = true; }
      if (strstr(request, offCmd)) { relayState[i] = false; actionRequested = true; }
    }
  }

  if (actionRequested) updateRelays();

  // --- ANTOWRT AN DEN CLIENT ---
  
  if (actionRequested) {
    // Wenn geschaltet wurde: Umleiten auf die Hauptseite (verhindert Re-Execution beim Refresh)
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
    client.println("Content-Length: 0");
    client.println("Connection: close");
    client.println();
  } 
  else {
    // Wenn nur die Hauptseite aufgerufen wird: HTML senden
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=UTF-8");
    client.println("Connection: close");
    client.println();
    
    client.println("<!DOCTYPE HTML><html><head>");
    // AUTO-REFRESH ALLE 5 SEKUNDEN:
    client.println("<meta http-equiv='refresh' content='5; URL=/'>");
    client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    client.println("<style>body{font-family:sans-serif; padding:20px; background:#f4f4f4;}");
    client.println(".card{background:white; padding:15px; margin-bottom:10px; border-radius:8px; box-shadow: 2px 2px 5px #ccc;}");
    client.println("button{padding:12px; margin:5px 0; cursor:pointer; width:100%; border:none; border-radius:4px; background:#007bff; color:white; font-size:16px;}");
    client.println(".status-on{color:green; font-weight:bold;} .status-off{color:red; font-weight:bold;}</style></head><body>");
    
    client.println("<h1>Relais Steuerung</h1>");

    for (byte i = 0; i < 8; i++) {
      client.println("<div class='card'>");
      client.print("<b>"); 
      client.print(relayNames[i]); 
      client.print(": </b>");
      client.print(relayState[i] ? "<span class='status-on'>AN</span>" : "<span class='status-off'>AUS</span>");
      client.println("<br>");

      if (i == 0) {
        client.println("<a href='/on1'><button>Impuls 2s</button></a>");
      } 
      else if (i == 1) {
        client.println("<a href='/on2_short'><button>Impuls 1s</button></a>");
        client.println("<a href='/on2_long'><button style='background:#17a2b8'>Impuls 10s</button></a>");
      } 
      else {
        client.print("<a href='/");
        client.print(relayState[i] ? "off" : "on");
        client.print(i + 1);
        client.print("'><button style='background:");
        client.print(relayState[i] ? "#dc3545" : "#28a745"); // Rot für Aus, Grün für An
        client.print("'>");
        client.print(relayState[i] ? "Ausschalten" : "Einschalten");
        client.println("</button></a>");
      }
      client.println("</div>");
    }

    client.println("</body></html>");
  }

  delay(1);
  client.stop();
}
