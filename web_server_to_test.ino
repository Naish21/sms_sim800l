#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <DNSServer.h>

// Configuration
const char* ap_ssid = "ESP32-SMS-Config";
const char* ap_password = "12345678";
const char* auth_user = "admin";
const char* auth_pass = "admin123";

// Objects
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// State
bool isAPMode = false;
String ssid = "";
String password = "";

// Function to check Basic Auth
bool checkAuth() {
  if (!server.authenticate(auth_user, auth_pass)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// Handle SMS POST request
void handleSMS() {
  if (!checkAuth()) return;
  
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }
  
  String body = server.arg("plain");
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  if (!doc.containsKey("destination_phone") || !doc.containsKey("message")) {
    server.send(400, "application/json", "{\"error\":\"Missing required fields\"}");
    return;
  }
  
  String phone = doc["destination_phone"].as<String>();
  String message = doc["message"].as<String>();
  
  // TODO: Implement your SMS sending logic here
  // For example, using a GSM module (SIM800L, SIM7600, etc.)
  Serial.println("SMS Request:");
  Serial.println("Phone: " + phone);
  Serial.println("Message: " + message);
  
  // Simulate SMS sending
  bool success = sendSMS(phone, message);
  
  if (success) {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"SMS sent\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send SMS\"}");
  }
}

// Placeholder for SMS sending function
bool sendSMS(String phone, String message) {
  // TODO: Implement actual SMS sending logic here
  // Example for SIM800L:
  // Serial2.println("AT+CMGF=1");
  // delay(100);
  // Serial2.println("AT+CMGS=\"" + phone + "\"");
  // delay(100);
  // Serial2.print(message);
  // delay(100);
  // Serial2.write(26);
  
  return true; // Simulate success
}

// Captive portal HTML page
const char* configPage = R"(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 WiFi Config</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; margin: 20px; }
    input { width: 100%; padding: 10px; margin: 5px 0; box-sizing: border-box; }
    button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; cursor: pointer; }
    button:hover { background: #45a049; }
  </style>
</head>
<body>
  <h2>ESP32 WiFi Configuration</h2>
  <form action="/save" method="POST">
    <label>WiFi SSID:</label>
    <input type="text" name="ssid" required>
    <label>WiFi Password:</label>
    <input type="password" name="password" required>
    <br><br>
    <button type="submit">Save & Restart</button>
  </form>
</body>
</html>
)";

// Handle captive portal root
void handleRoot() {
  server.send(200, "text/html", configPage);
}

// Handle WiFi credentials save
void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    
    preferences.begin("wifi", false);
    preferences.putString("ssid", newSSID);
    preferences.putString("password", newPassword);
    preferences.end();
    
    server.send(200, "text/html", 
      "<html><body><h2>Configuration Saved!</h2><p>ESP32 will restart and connect to your network.</p></body></html>");
    
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<html><body><h2>Error</h2><p>Missing parameters</p></body></html>");
  }
}

// Start Access Point mode
void startAPMode() {
  Serial.println("Starting AP Mode...");
  isAPMode = true;
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Start DNS server for captive portal
  dnsServer.start(53, "*", IP);
  
  // Setup captive portal routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot); // Redirect all to root
  
  server.begin();
  Serial.println("Captive portal started");
}

// Start Station mode
void startStationMode() {
  Serial.println("Connecting to WiFi...");
  isAPMode = false;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup API routes
    server.on("/sms", HTTP_POST, handleSMS);
    server.onNotFound([]() {
      server.send(404, "application/json", "{\"error\":\"Not found\"}");
    });
    
    server.begin();
    Serial.println("SMS API started");
  } else {
    Serial.println("\nFailed to connect. Starting AP mode...");
    startAPMode();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 SMS API Starting...");
  
  // Load WiFi credentials
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
  
  // Check if credentials exist
  if (ssid.length() > 0) {
    startStationMode();
  } else {
    startAPMode();
  }
}

void loop() {
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
  delay(1);
}
