// Erweiterter ESP8266-Code mit zwei WLANs im EEPROM + MQTT-Konfiguration

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#define DHTPIN 14 // D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define EEPROM_SIZE 192

// EEPROM-Adressen
#define SSID1_ADDR   0
#define PASS1_ADDR   32
#define SSID2_ADDR   64
#define PASS2_ADDR   96
#define MQTT_ADDR    128
#define INTERVAL_ADDR 160
#define OTA_FLAG_ADDR 164

// Standardwerte (nur beim ersten Flashen verwendet)
const char* default_ssid1 = "FRITZ!Box 6660 Cable TL";
const char* default_pass1 = "ASTSJSFS2023!";
const char* default_ssid2 = "Fritz600CP";
const char* default_pass2 = "ASTSJSFS2023!";

char ssid1[32], pass1[32], ssid2[32], pass2[32];
char mqtt_server[32] = "192.168.178.110";
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt";
const char* mqtt_pass = "Penelope01!";

unsigned long updateInterval = 300; // Sekunden
bool otaRequested = false;

const char* topic_temp = "esp8266/temperature";
const char* topic_hum = "esp8266/humidity";
const char* topic_cfg = "esp8266/config";
const char* topic_ota = "esp8266/ota";
const char* topic_wifi1 = "esp8266/wifi1";
const char* topic_wifi2 = "esp8266/wifi2";

WiFiClient espClient;
PubSubClient client(espClient);

void writeString(int addr, const char* str) {
  for (int i = 0; i < 32; ++i)
    EEPROM.write(addr + i, str[i] ? str[i] : 0);
}

void readString(int addr, char* buffer) {
  for (int i = 0; i < 32; ++i)
    buffer[i] = EEPROM.read(addr + i);
  buffer[31] = 0;
}

void saveCredentials() {
  writeString(SSID1_ADDR, default_ssid1);
  writeString(PASS1_ADDR, default_pass1);
  writeString(SSID2_ADDR, default_ssid2);
  writeString(PASS2_ADDR, default_pass2);
  writeString(MQTT_ADDR, mqtt_server);
  EEPROM.put(INTERVAL_ADDR, updateInterval);
  EEPROM.write(OTA_FLAG_ADDR, 0);
  EEPROM.commit();
}

void loadCredentials() {
  readString(SSID1_ADDR, ssid1);
  readString(PASS1_ADDR, pass1);
  readString(SSID2_ADDR, ssid2);
  readString(PASS2_ADDR, pass2);
  readString(MQTT_ADDR, mqtt_server);
  EEPROM.get(INTERVAL_ADDR, updateInterval);
  if (updateInterval < 10 || updateInterval > 86400) updateInterval = 60;
  otaRequested = EEPROM.read(OTA_FLAG_ADDR) == 1;
}

bool connectToWiFi() {
  WiFi.mode(WIFI_STA);

  Serial.printf("Verbinde mit %s...\n", ssid1);
  WiFi.begin(ssid1, pass1);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500); Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWLAN 1 verbunden.");
    return true;
  }

  Serial.printf("\nWLAN 1 fehlgeschlagen – versuche %s...\n", ssid2);
  WiFi.begin(ssid2, pass2);
  start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500); Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWLAN 2 verbunden.");
    return true;
  }

  Serial.println("\nWLAN-Verbindung fehlgeschlagen.");
  return false;
}

void saveOtaFlag(bool value) {
  EEPROM.write(OTA_FLAG_ADDR, value ? 1 : 0);
  EEPROM.commit();
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == topic_cfg) {
    unsigned long val = msg.toInt();
    if (val >= 10 && val <= 86400) {
      EEPROM.put(INTERVAL_ADDR, val);
      EEPROM.commit();
      updateInterval = val;
      Serial.printf("Neues Intervall: %lus\n", val);
    }
  } else if (String(topic) == topic_ota) {
    if (msg == "on") {
      saveOtaFlag(true);
      otaRequested = true;
      Serial.println("OTA aktiviert");
    } else {
      saveOtaFlag(false);
      otaRequested = false;
      Serial.println("OTA deaktiviert");
    }
  } else if (String(topic) == topic_wifi1 || String(topic) == topic_wifi2) {
    int sep = msg.indexOf(';');
    if (sep > 0 && sep < msg.length() - 1) {
      String s = msg.substring(0, sep);
      String p = msg.substring(sep + 1);
      if (topic == topic_wifi1) {
        writeString(SSID1_ADDR, s.c_str());
        writeString(PASS1_ADDR, p.c_str());
      } else {
        writeString(SSID2_ADDR, s.c_str());
        writeString(PASS2_ADDR, p.c_str());
      }
      EEPROM.commit();
      Serial.printf("WLAN %s gespeichert: %s / %s\n", topic == topic_wifi1 ? "1" : "2", s.c_str(), p.c_str());
    }
  }
}

void sendToMQTT(float t, float h) {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  if (client.connect("ESP8266Client", mqtt_user, mqtt_pass)) {
    char tempStr[8], humStr[8];
    dtostrf(t, 6, 2, tempStr);
    dtostrf(h, 6, 2, humStr);
    client.publish(topic_temp, tempStr);
    client.publish(topic_hum, humStr);
    client.subscribe(topic_cfg);
    client.subscribe(topic_ota);
    client.subscribe(topic_wifi1);
    client.subscribe(topic_wifi2);
    delay(2000);
    client.loop();
    client.disconnect();
  }
}

void setupOTA() {
  ArduinoOTA.setHostname("esp8266");
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);

  // Nur beim ersten Flashen nötig:
  //saveCredentials();
  
  Serial.println("loadCredentials...");
  loadCredentials();
  Serial.print("UpdateInterval: "); Serial.println(updateInterval);
  dht.begin();

  Serial.println("load WIFI...");
  if (connectToWiFi()) {
    Serial.println("  Setup OTA...");
    setupOTA();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    Serial.print("  DHT Luftfeuchtigkeit: ");
    Serial.println(h);
    Serial.print("  DHT Temperatur: ");
    Serial.println(t);
    if (!isnan(t) && !isnan(h)) {
      Serial.println("    Sende Daten...");
      sendToMQTT(t, h);
    }
    if (otaRequested) {
      Serial.println("OTA-Modus 3 Minuten aktiv...");
      unsigned long start = millis();
      while (millis() - start < 180000) {
        ArduinoOTA.handle();
        delay(10);
      }
      Serial.println("OTA beendet.");
      saveOtaFlag(false);
    }
    Serial.println("Alles erledigt... DeepSleep");
    ESP.deepSleep(updateInterval * 1e6);
  } else {
    Serial.println("Kein WLAN verbunden... DeepSleep");
    ESP.deepSleep(updateInterval * 1e6);
  }
}

void loop() {
  // nicht verwendet
}
