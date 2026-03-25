/*
Project: ESP MQTT Matrix Clock
Description: MAX7219 Internet Clock (GFX Version) + MQTT
Compatible with: ESP8266 and ESP32
Libraries: Adafruit GFX, Max72xxPanel, ElegantOTA, WiFiManager, PubSubClient
*/

#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #include <FS.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
#endif

#include <ElegantOTA.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <time.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

// ================= HARDWARE =================
#ifdef ESP32
  #define CS_PIN   7  // Typical for ESP32-C3 Super Mini (GPIO 7)
#else
  #define CS_PIN   D6 // Default for NodeMCU/D1 Mini
#endif

#define H_DISPLAYS 4
#define V_DISPLAYS 1

// ================= SETTINGS =================
char ntpServer[64] = "pool.ntp.org";
char customHostname[32] = "";
long gmtOffset_sec = 28800;
uint8_t displayIntensity = 1;
uint8_t activeDevices = 4;
bool is24h = true;

// --- MQTT Settings ---
char mqttServer[64] = "";
int  mqttPort = 1883;
char mqttUser[32] = "";
char mqttPassword[32] = "";
unsigned long lastTeleTick = 0;
int telePeriod = 60; 

// ================= STATUS FLAGS =================
bool ntpSynced = false;
unsigned long lastNtpCheck = 0;
const unsigned long ntpCheckInterval = 10000; 

bool mqttConnected = false;
unsigned long lastMqttChange = 0;

// --------------------------

char szTime[14] = ""; 
#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif

WiFiClient espClient;
PubSubClient client(espClient); 

// Initialize GFX Matrix
Max72xxPanel matrix = Max72xxPanel(CS_PIN, H_DISPLAYS, V_DISPLAYS);

unsigned long lastSecondTick = 0;
String incomingMqttMessage = ""; 

// ================= HELPERS =================
String getUniqueId() {
#ifdef ESP32
  return String((uint32_t)ESP.getEfuseMac(), HEX);
#else
  return String(ESP.getChipId(), HEX);
#endif
}

// ================= FILESYSTEM =================
void saveSettings() {
  File f = LittleFS.open("/config.txt", "w");
  if (!f) return;
  f.println(ntpServer);
  f.println(customHostname);
  f.println(activeDevices);
  f.println(gmtOffset_sec);
  f.println(displayIntensity);
  f.println(is24h ? "1" : "0");
  f.println(mqttServer);
  f.println(mqttPort);
  f.println(mqttUser);
  f.println(mqttPassword);
  f.println(telePeriod); 
  f.close();
}

void loadSettings() {
  if (!LittleFS.exists("/config.txt")) return;
  File f = LittleFS.open("/config.txt", "r");
  if (!f) return;
  String s;
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) s.toCharArray(ntpServer, 64);
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) s.toCharArray(customHostname, 32);
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) activeDevices = constrain(s.toInt(), 2, H_DISPLAYS);
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) gmtOffset_sec = s.toInt();
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) displayIntensity = constrain(s.toInt(), 0, 15);
  s = f.readStringUntil('\n'); s.trim(); is24h = (s == "1");

  s = f.readStringUntil('\n'); s.trim(); if (s.length()) s.toCharArray(mqttServer, 64);
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) mqttPort = s.toInt();
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) s.toCharArray(mqttUser, 32);
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) s.toCharArray(mqttPassword, 32);
  s = f.readStringUntil('\n'); s.trim(); if (s.length()) telePeriod = s.toInt();
  
  f.close();
}


// ================= CHECK NTP STATUS =================

void checkNtpStatus() {
  if (millis() - lastNtpCheck < ntpCheckInterval) return;
  lastNtpCheck = millis();

  struct tm t;
  if (getLocalTime(&t)) {
    if (!ntpSynced) {
      ntpSynced = true;
      Serial.println("NTP synced");
    }
  } else {
    if (ntpSynced) {
      ntpSynced = false;
      Serial.println("NTP lost");
    }
  }
}


// ================= GFX HELPER LOGIC =================

void configureMatrixLayout() {
  matrix.setIntensity(displayIntensity);
  for(int i=0; i<H_DISPLAYS; i++) {
    matrix.setRotation(i, 1); 
  }
}

void drawCenteredText(String text, bool showPm = false) {
  int16_t x1, y1;
  uint16_t w, h;
  int screenWidth = activeDevices * 8; 

  matrix.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (screenWidth - w) / 2;
  int y = 0; 

  matrix.fillScreen(LOW);
  matrix.setCursor(x, y);
  matrix.print(text);

  if (showPm) {
    int colonIndex = text.indexOf(':');
    if (colonIndex == -1) colonIndex = text.indexOf(' '); 
    if (colonIndex != -1) {
      int dotX = x + (colonIndex * 6) + 2;
      matrix.drawPixel(dotX, 7, HIGH); 
    }
  }
  matrix.write(); 
}

void scrollTextBlocking(String text) {
  int width = text.length() * 6; 
  int screenWidth = activeDevices * 8;
  
  for (int x = screenWidth; x >= -width; x--) {
    matrix.fillScreen(LOW);
    matrix.setCursor(x, 0);
    matrix.print(text);
    matrix.write();
    delay(40); 
    yield();    
  }
}

// ================= MQTT LOGIC =================

void sendTelemetry() {
  if (!client.connected()) return;

  String topic = "tele/" + String(customHostname) + "/STATE";

  String payload = "{";
  payload += "\"Hostname\":\"" + String(customHostname) + "\",";
  payload += "\"IP\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"NTP\":\"" + String(ntpSynced ? "OK" : "FAIL") + "\",";
  payload += "}";

  client.publish(topic.c_str(), payload.c_str());
}

// ================= MQTT CALLBACK =================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("MQTT Received on [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  incomingMqttMessage = message;
}

void reconnectMqtt() {
  if (strlen(mqttServer) == 0) return;

  String clientId = String(customHostname) + "-" + getUniqueId();

  bool connected = (strlen(mqttUser) > 0)
      ? client.connect(clientId.c_str(), mqttUser, mqttPassword)
      : client.connect(clientId.c_str());

  if (connected) {
    mqttConnected = true;
    lastMqttChange = millis();
    Serial.println("MQTT connected");

    String topic = "cmnd/" + String(customHostname) + "/DisplayText";
    client.subscribe(topic.c_str());
    sendTelemetry();
  } else {
    mqttConnected = false;
  }
}

void showStatusIfNeeded() {
  static bool shown = false;
  if (shown) return;

  if (!ntpSynced) {
    scrollTextBlocking("NTP ERR");
    shown = true;
  } 
  else if (strlen(mqttServer) > 0 && !mqttConnected) {
    scrollTextBlocking("MQTT OFF");
    shown = true;
  }
}

// ================= DISPLAY LOGIC =================

void updateDisplay() {
  struct tm t;
  if (!getLocalTime(&t)) return;
  
  char colon = (t.tm_sec % 2 == 0) ? ':' : ' ';
  char timeBuff[10];
  bool isPm = false; 

  if (activeDevices < 4) {
    bool toggle = (t.tm_sec % 4 < 2);
    if (activeDevices == 2) {
      if (toggle) sprintf(timeBuff, "%02d", t.tm_hour);
      else sprintf(timeBuff, ":%02d", t.tm_min);
    } else {
      if (toggle) sprintf(timeBuff, "%02dh", t.tm_hour);
      else sprintf(timeBuff, "%02dm", t.tm_min);
    }
  } else {
    if (is24h) {
      sprintf(timeBuff, "%02d%c%02d", t.tm_hour, colon, t.tm_min);
    } else {
      int h12 = t.tm_hour % 12; 
      if (h12 == 0) h12 = 12;
      if (t.tm_hour >= 12) isPm = true;
      sprintf(timeBuff, "%d%c%02d", h12, colon, t.tm_min);
    }
  }
  
  drawCenteredText(String(timeBuff), isPm);
}

// ================= WEB UI =================
String getHTML() {
  int h_part = gmtOffset_sec / 3600;
  int m_part = abs(gmtOffset_sec % 3600) / 60;
  
  String fullMqttTopic = "cmnd/" + String(customHostname) + "/DisplayText";

  String h = "<html><head><title>Clock Config</title><meta name='viewport' content='width=device-width, initial-scale=1'>";
  
  h += "<style>";
  h += "body{font-family:sans-serif;padding:20px;background:#f4f4f4;}";
  h += ".card{background:white;padding:20px;border-radius:8px;max-width:400px;margin:auto;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  h += "input, select, .btn {width:100%; padding:12px; margin:8px 0; border:1px solid #ccc; border-radius:4px; box-sizing:border-box; font-size:16px;}";
  h += ".btn {color:white; border:none; cursor:pointer; text-decoration:none; display:block; text-align:center;}";
  h += ".save{background:#4CAF50;}";
  h += ".reboot{background:#FF9800;}"; 
  h += ".ota{background:#2196F3;}"; 
  h += ".reset{background:#f44336; margin-top:30px;}";
  h += ".grid{display:flex; gap:10px;}"; 
  h += ".mqtt-info{background:#e3f2fd; padding:10px; border-radius:4px; border-left:4px solid #2196F3; font-family:monospace; font-size:14px; word-break:break-all; margin-bottom:10px;}";
  h += "</style></head><body><div class='card'><h2>Clock Settings</h2>";
  
  h += "<form action='/save' method='POST'>";
  h += "<label>Hostname:</label><input name='host' value='" + String(customHostname) + "'>";
  h += "<label>NTP Server:</label><input name='ntp' value='" + String(ntpServer) + "'>";

  h += "<label>Modules (2-4):</label><input type='number' min='2' max='4' name='devices' value='" + String(activeDevices) + "'>";
  h += "<label>Time Format:</label><select name='mode'><option value='24' "+String(is24h?"selected":"")+">24-Hour</option><option value='12' "+String(!is24h?"selected":"")+">12-Hour</option></select>";
  h += "<label>Brightness (0-15):</label><input type='number' min='0' max='15' name='brightness' value='"+String(displayIntensity)+"'>";
  
  h += "<label>GMT Offset (Hr / Min):</label><div class='grid'>";
  h += "<input type='number' min='-12' max='14' name='tzH' value='"+String(h_part)+"' placeholder='Hr'>";
  h += "<input type='number' min='0' max='59' name='tzM' value='"+String(m_part)+"' placeholder='Min'></div>";

  h += "<hr><h3>MQTT Config</h3>";
  h += "<p style='font-size:12px; color:#666; margin-bottom:5px;'>Publish message to:</p>";  
  h += "<div class='mqtt-info'>" + fullMqttTopic + "</div>";

  h += "<label>Broker IP/URL:</label><input name='mqServer' value='" + String(mqttServer) + "' placeholder='192.168.1.X'>";
  h += "<label>Port:</label><input name='mqPort' type='number' value='" + String(mqttPort) + "'>";
  h += "<label>User (Optional):</label><input name='mqUser' value='" + String(mqttUser) + "'>";
  h += "<label>Password (Optional):</label><input name='mqPass' type='password' value='" + String(mqttPassword) + "'>";
  
  h += "<input type='submit' value='Save & Apply' class='btn save'></form>";
  h += "<a href='/reboot' class='btn reboot'>Reboot Device</a>";
  h += "<a href='/update' class='btn ota'>OTA Update</a>";
  h += "<form action='/factory' method='POST' onsubmit='return confirm(\"Reset all?\")'><input type='submit' value='FACTORY RESET' class='btn reset'></form></div></body></html>";
  return h;
}

// ================= WIFI MANAGER CALLBACK =================
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  String ssid = myWiFiManager->getConfigPortalSSID();
  scrollTextBlocking("Connect to WiFi SSID: " + ssid);
  drawCenteredText("SETUP"); 
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  loadSettings(); 

  configureMatrixLayout();
  matrix.fillScreen(LOW);

  WiFiManager wm;
  wm.setAPCallback(configModeCallback); 

  if (strlen(customHostname) == 0) {
    String host = "Clock-" + getUniqueId();
    host.toCharArray(customHostname, 32);
  }
  
  wm.setConnectTimeout(30); 

  if (!wm.autoConnect(customHostname)) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }

  configTime(gmtOffset_sec, 0, ntpServer);

  if (strlen(mqttServer) > 0) {
    client.setServer(mqttServer, mqttPort);
    client.setCallback(mqttCallback);
  }

  scrollTextBlocking("IP:" + WiFi.localIP().toString());

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { 
      scrollTextBlocking("NTP Error: Check URL");
  }
  
  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("host")) server.arg("host").toCharArray(customHostname, 32);
    if (server.hasArg("ntp")) server.arg("ntp").toCharArray(ntpServer, 64);
    if (server.hasArg("mode")) is24h = (server.arg("mode") == "24");
    
    if (server.hasArg("devices")) {
      activeDevices = constrain(server.arg("devices").toInt(), 2, H_DISPLAYS);
    }

    if (server.hasArg("brightness")) {
      displayIntensity = constrain(server.arg("brightness").toInt(), 0, 15);
      matrix.setIntensity(displayIntensity);
    }
    
    if (server.hasArg("tzH")) {
      long h = constrain(server.arg("tzH").toInt(), -12, 14);
      long m = constrain(server.arg("tzM").toInt(), 0, 59);
      if (h < 0) gmtOffset_sec = (h * 3600) - (m * 60);
      else gmtOffset_sec = (h * 3600) + (m * 60);
      configTime(gmtOffset_sec, 0, ntpServer);
    }

    if (server.hasArg("mqServer")) server.arg("mqServer").toCharArray(mqttServer, 64);
    if (server.hasArg("mqPort")) mqttPort = server.arg("mqPort").toInt();
    if (server.hasArg("mqUser")) server.arg("mqUser").toCharArray(mqttUser, 32);
    if (server.hasArg("mqPass")) server.arg("mqPass").toCharArray(mqttPassword, 32);

    saveSettings(); 
    
    if (strlen(mqttServer) > 0) {
       client.setServer(mqttServer, mqttPort);
       client.setCallback(mqttCallback);
    }

    server.send(200, "text/html", "<h2>Saved!</h2><a href='/'>Back</a>");
    updateDisplay();
  });

  server.on("/reboot", []() {
    server.send(200, "text/html", "<h2>Rebooting...</h2>");
    delay(1000); ESP.restart();
  });

  server.on("/factory", HTTP_POST, []() {
    server.send(200, "text/html", "<h2>Resetting...</h2>");
    WiFiManager wm; wm.resetSettings(); LittleFS.format(); ESP.restart();
  });

  ElegantOTA.begin(&server);
  server.begin();
  updateDisplay(); 
}

void monitorConnections() {
  static unsigned long lastStatusCheck = 0;
  if (millis() - lastStatusCheck < 30000) return;
  lastStatusCheck = millis();

  if (WiFi.status() == WL_CONNECTED && !ntpSynced) {
    scrollTextBlocking("NTP Error");
  }
}

void loop() {
  server.handleClient();
  ElegantOTA.loop();
  
  checkNtpStatus(); 
  
  if (strlen(mqttServer) > 0) {
    if (!client.connected()) {
       static unsigned long lastMqttAttempt = 0;
       if (millis() - lastMqttAttempt > 5000) {
         lastMqttAttempt = millis();
         reconnectMqtt();
       }
    }
    client.loop(); 
    if (telePeriod > 0 && (millis() - lastTeleTick >= (telePeriod * 1000))) {
      lastTeleTick = millis();
      sendTelemetry();
    }
    
    if (mqttConnected != client.connected()) {
      mqttConnected = client.connected();
      lastMqttChange = millis();
    }
  }

  if (incomingMqttMessage != "") {
    scrollTextBlocking(incomingMqttMessage);
    incomingMqttMessage = ""; 
    updateDisplay(); 
    lastSecondTick = millis(); 
  }
  
  if (millis() - lastSecondTick >= 1000) {
    lastSecondTick = millis();
    updateDisplay();
  }

  monitorConnections(); 
}
