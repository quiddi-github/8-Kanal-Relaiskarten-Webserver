#include <SPI.h>
#include <Ethernet.h>

/* ======================= Hardware-Konfiguration ======================= */
#define W5500_CS 53

// Relais-Pins (HIGH-aktiv)
const byte relayPins[8] = {22, 23, 24, 25, 26, 27, 28, 29};

// Taster-Pins (INPUT_PULLUP, gegen GND)
const byte buttonPins[8] = {32, 33, 34, 35, 36, 37, 38, 39};

/* ============================================================
   BESCHRIFTUNG
   ============================================================ */
const char* relayNames[8] = {
  "NAS Power-Button",      // Relais 1 (Impuls 2s)
  "Proxmox-Server",        // Relais 2 (Impuls 1s / 10s)
  "Telefon DECT Station",  // Relais 3
  "Relais 4",              // Relais 4
  "Relais 5",
  "Relais 6",
  "Relais 7",
  "Relais 8"
};

/* ======================= Netzwerk-Konfiguration ======================= */
byte mac[] = { 0x1E, 0xAD, 0x2E, 0xEF, 0xFE, 0xEA };
IPAddress ip(192, 168, 0, 199);
IPAddress gateway(192, 168, 0, 253);
IPAddress subnet(255, 255, 255, 0);

EthernetServer server(80);

// Netzwerk-Statusvariablen
bool networkInitialized = false;
unsigned long lastNetworkCheck = 0;
const unsigned long NETWORK_CHECK_INTERVAL = 5000; // Prüfung alle 5 Sekunden

/* ======================= Statusvariablen ======================= */
bool relayState[8] = { false, false, false, false, false, false, false, false };
bool lastButtonState[8] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };
bool lastStableButtonState[8] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };

unsigned long relay1Timer = 0;
unsigned long relay1Duration = 2000;
unsigned long relay2Timer = 0;
unsigned long relay2Duration = 1000;

// Neu: Status für Relais 2 - welcher Modus aktiv ist
bool relay2ShortActive = false; // 1 Sekunde Modus
bool relay2LongActive = false;  // 10 Sekunden Modus

/* ======================= Entprellungsvariablen ======================= */
unsigned long lastDebounceTime[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
const unsigned long DEBOUNCE_DELAY = 50;

void updateRelays() {
  for (byte i = 0; i < 8; i++) {
    digitalWrite(relayPins[i], relayState[i] ? HIGH : LOW);
  }
}

/* ======================= Netzwerk-Funktionen ======================= */

// Prüft, ob das Netzwerk verfügbar ist
bool isNetworkAvailable() {
  // Methode 1: Prüfe, ob die Link-Status LED des W5500 leuchtet
  // (Hardware-abhängig, manche W5500 haben keine API dafür)
  
  // Methode 2: Versuche einen Ping an den Gateway
  // (Nicht direkt möglich mit Standard-Ethernet-Bibliothek)
  
  // Methode 3: Prüfe, ob die lokale IP-Adresse gültig ist
  // (Funktioniert nur, wenn die IP statisch ist)
  return Ethernet.localIP() == ip;
}

// Initialisiert das Netzwerk neu
bool initializeNetwork() {
  Serial.println("Versuche Netzwerk neu zu initialisieren...");
  
  // Ethernet-Stack zurücksetzen
  Ethernet.init(W5500_CS);
  delay(100);
  
  // Neu initialisieren
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  delay(1000);
  
  // Server neu starten
  server.begin();
  delay(100);
  
  // Prüfen ob erfolgreich
  if (Ethernet.localIP() == ip) {
    Serial.print("Netzwerk erfolgreich neu initialisiert. IP: ");
    Serial.println(Ethernet.localIP());
    networkInitialized = true;
    return true;
  } else {
    Serial.println("Netzwerk-Initialisierung fehlgeschlagen!");
    networkInitialized = false;
    return false;
  }
}

void setup() {
  Serial.begin(9600);
  
  // Relais initialisieren
  for (byte i = 0; i < 8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  
  // Taster initialisieren
  for (byte i = 0; i < 8; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonState[i] = digitalRead(buttonPins[i]);
    lastStableButtonState[i] = lastButtonState[i];
  }
  
  // Ethernet initialisieren
  pinMode(53, OUTPUT);
  
  // Netzwerk initialisieren
  if (initializeNetwork()) {
    Serial.println("Webserver bereit");
  } else {
    Serial.println("Netzwerk-Initialisierung fehlgeschlagen. Warte auf Verbindung...");
  }
}

void loop() {
  // ======================= NETZWERK-ÜBERWACHUNG =======================
  // Regelmäßig prüfen, ob das Netzwerk noch verfügbar ist
  if (millis() - lastNetworkCheck >= NETWORK_CHECK_INTERVAL) {
    lastNetworkCheck = millis();
    
    // Prüfe, ob Netzwerk verfügbar ist
    if (!isNetworkAvailable()) {
      Serial.println("Netzwerk nicht verfügbar! Versuche Neuinitialisierung...");
      
      // Versuche Netzwerk neu zu initialisieren
      if (initializeNetwork()) {
        Serial.println("Netzwerk wiederhergestellt!");
      } else {
        Serial.println("Netzwerk-Wiederherstellung fehlgeschlagen. Versuche später erneut...");
      }
    }
  }
  
  // ======================= AUTO-OFF RELAIS =======================
  // Auto-OFF Relais 1
  if (relayState[0] && (millis() - relay1Timer >= relay1Duration)) {
    relayState[0] = false;
    digitalWrite(relayPins[0], LOW);
  }
  
  // Auto-OFF Relais 2 - mit separatem Reset der Modus-Flags
  if (relayState[1] && (millis() - relay2Timer >= relay2Duration)) {
    relayState[1] = false;
    relay2ShortActive = false;
    relay2LongActive = false;
    digitalWrite(relayPins[1], LOW);
  }
  
  // ======================= TASTER ABFRAGEN =======================
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
          } else if (i == 1) {
            relayState[1] = true;
            relay2Duration = 1000;
            relay2Timer = millis();
            relay2ShortActive = true;  // Taster = kurzer Impuls
            relay2LongActive = false;
          } else {
            relayState[i] = !relayState[i];
          }
          digitalWrite(relayPins[i], relayState[i] ? HIGH : LOW);
        }
      }
    }
    lastButtonState[i] = currentReading;
  }
  
  // ======================= WEBSERVER =======================
  // Nur Clients annehmen, wenn Netzwerk initialisiert ist
  if (!networkInitialized) {
    return;
  }
  
  EthernetClient client = server.available();
  if (!client) {
    return;
  }
  
  // Client-Timeout einstellen (verhindert Blockierung)
  unsigned long clientTimeout = millis() + 5000; // 5 Sekunden Timeout
  
  char request[100];
  int index = 0;
  
  while (client.connected() && index < 99 && millis() < clientTimeout) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        break;
      }
      request[index++] = c;
    }
  }
  request[index] = '\0';
  
  // Wenn Client nicht in Timeout war, Anfrage verarbeiten
  if (millis() < clientTimeout) {
    bool actionRequested = false;
    
    // Befehle verarbeiten
    if (strstr(request, "GET /on1")) {
      relayState[0] = true;
      relay1Duration = 2000;
      relay1Timer = millis();
      actionRequested = true;
    } else if (strstr(request, "GET /on2_short")) {
      relayState[1] = true;
      relay2Duration = 1000;
      relay2Timer = millis();
      relay2ShortActive = true;
      relay2LongActive = false;
      actionRequested = true;
    } else if (strstr(request, "GET /on2_long")) {
      relayState[1] = true;
      relay2Duration = 10000;
      relay2Timer = millis();
      relay2ShortActive = false;
      relay2LongActive = true;
      actionRequested = true;
    } else {
      for (byte i = 2; i < 8; i++) {
        char onCmd[12], offCmd[12];
        sprintf(onCmd, "GET /on%d", i + 1);
        sprintf(offCmd, "GET /off%d", i + 1);
        
        if (strstr(request, onCmd)) {
          relayState[i] = true;
          actionRequested = true;
        }
        if (strstr(request, offCmd)) {
          relayState[i] = false;
          actionRequested = true;
        }
      }
    }
    
    if (actionRequested) {
      updateRelays();
    }
    
    // ======================= ANTWORT AN DEN CLIENT =======================
    if (actionRequested) {
      // Bei Aktion: Umleitung zur Hauptseite
      client.println("HTTP/1.1 303 See Other");
      client.println("Location: /");
      client.println("Content-Length: 0");
      client.println("Connection: close");
      client.println();
    } else {
      // Normale HTML-Seite
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html; charset=UTF-8");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML><html><head>");
      client.println("<meta http-equiv='refresh' content='5; URL=/'>");
      client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
      client.println("<style>body{font-family:sans-serif; padding:20px; background:#f4f4f4;}");
      client.println(".card{background:white; padding:15px; margin-bottom:10px; border-radius:8px; box-shadow: 2px 2px 5px #ccc;}");
      client.println("button{padding:12px; margin:5px 0; cursor:pointer; width:100%; border:none; border-radius:4px; color:white; font-size:16px;}");
      client.println(".status-on{color:green; font-weight:bold;} .status-off{color:red; font-weight:bold;}");
      client.println("</style></head><body>");
      client.println("<h1>Relais Steuerung</h1>");
      
      for (byte i = 0; i < 8; i++) {
        client.println("<div class='card'>");
        client.print("<b>");
        client.print(relayNames[i]);
        client.print(": </b>");
        client.print(relayState[i] ? "<span class='status-on'>AN</span>" : "<span class='status-off'>AUS</span>");
        client.println("<br>");
        
        if (i == 0) {
          // Relais 1 - Button wird rot wenn aktiv
          client.print("<a href='/on1'><button style='background:");
          client.print(relayState[i] ? "#dc3545" : "#007bff");
          client.print("'>Impuls 2s</button></a>");
        } else if (i == 1) {
          // Relais 2 - Jeder Button wird nur rot wenn er spezifisch aktiv ist
          client.print("<a href='/on2_short'><button style='background:");
          client.print(relay2ShortActive ? "#dc3545" : "#007bff");
          client.print("'>Impuls 1s</button></a>");
          client.print("<a href='/on2_long'><button style='background:");
          client.print(relay2LongActive ? "#dc3545" : "#17a2b8");
          client.print("'>Impuls 10s</button></a>");
        } else {
          client.print("<a href='/");
          client.print(relayState[i] ? "off" : "on");
          client.print(i + 1);
          client.print("'><button style='background:");
          client.print(relayState[i] ? "#dc3545" : "#28a745");
          client.print("'>");
          client.print(relayState[i] ? "Ausschalten" : "Einschalten");
          client.println("</button></a>");
        }
        client.println("</div>");
      }
      client.println("</body></html>");
    }
  }
  
  delay(1);
  client.stop();
}
