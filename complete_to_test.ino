#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <DNSServer.h>

// SIM800L Pin Configuration
#define RXD2 16
#define TXD2 17

// WiFi Configuration
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
bool sim800lReady = false;
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

// Initialize SIM800L module
void initSIM800L() {
  Serial.println("Initializing SIM800L (10s)...");
  delay(10000);
  
  Serial1.println("AT");
  delay(1000);
  if (waitForResponse("OK", 2000)) {
    Serial.println("SIM800L: Handshake OK");
  } else {
    Serial.println("SIM800L: Handshake failed");
    return;
  }
  
  Serial1.println("AT+CSQ");
  delay(1000);
  Serial.println("Signal quality check:");
  printSerialResponse();
  
  Serial1.println("AT+CCID");
  delay(1000);
  Serial.println("SIM card info:");
  printSerialResponse();
  
  Serial1.println("AT+CREG?");
  delay(1000);
  Serial.println("Network registration:");
  printSerialResponse();
  
  Serial1.println("AT+CMGF=1"); // Set SMS text mode
  delay(1000);
  if (waitForResponse("OK", 2000)) {
    Serial.println("SIM800L: SMS mode configured");
    sim800lReady = true;
  } else {
    Serial.println("SIM800L: Failed to configure SMS mode");
  }
}

// Wait for specific response from SIM800L
bool waitForResponse(String expected, unsigned long timeout) {
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    while (Serial1.available()) {
      char c = Serial1.read();
      response += c;
      Serial.write(c);
    }
    if (response.indexOf(expected) != -1) {
      return true;
    }
    delay(10);
  }
  return false;
}

// Print response from SIM800L
void printSerialResponse() {
  while (Serial1.available()) {
    Serial.write(Serial1.read());
  }
}

// Send SMS via SIM800L
bool sendSMS(String phone, String message) {
  if (!sim800lReady) {
    Serial.println("SIM800L not ready");
    return false;
  }
  
  Serial.println("Sending SMS to: " + phone);
  Serial.println("Message: " + message);
  
  // Clear any pending data
  while (Serial1.available()) {
    Serial1.read();
  }
  
  // Set text mode
  Serial1.println("AT+CMGF=1");
  delay(500);
  if (!waitForResponse("OK", 2000)) {
    Serial.println("Failed to set text mode");
    return false;
  }
  
  // Set recipient
  Serial1.println("AT+CMGS=\"" + phone + "\"");
  delay(500);
  if (!waitForResponse(">", 3000)) {
    Serial.println("Failed to set recipient");
    return false;
  }
  
  // Send message content
  Serial1.println(message);
  delay(500);
  
  // Send Ctrl+Z to finish
  Serial1.write(26);
  delay(500);
  
  // Wait for confirmation
  if (waitForResponse("OK", 10000)) {
    Serial.println("SMS sent successfully");
    return true;
  } else {
    Serial.println("SMS sending failed");
    return false;
  }
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
    server.send(400, "application/json", "{\"error\":\"Missing required fields: destination_phone and message\"}");
    return;
  }
  
  String phone = doc["destination_phone"].as<String>();
  String message = doc["message"].as<String>();
  
  if (phone.length() == 0 || message.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Empty phone or message\"}");
    return;
  }
  
  // Add + prefix if not present and phone doesn't start with it
  if (phone.charAt(0) != '+') {
    phone = "+" + phone;
  }
  
  bool success = sendSMS(phone, message);
  
  if (success) {
    StaticJsonDocument<128> responseDoc;
    responseDoc["status"] = "success";
    responseDoc["message"] = "SMS sent";
    responseDoc["destination"] = phone;
    
    String response;
    serializeJson(responseDoc, response);
    server.send(200, "application/json", response);
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to send SMS\"}");
  }
}

// Check SIM800L status endpoint
void handleStatus() {
  if (!checkAuth()) return;
  
  StaticJsonDocument<256> doc;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi_ip"] = WiFi.localIP().toString();
  doc["sim800l_ready"] = sim800lReady;
  doc["ap_mode"] = isAPMode;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Captive portal HTML page
const char* configPage = R"(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 WiFi Config</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; margin: 20px; background: #f0f0f0; }
    .container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h2 { color: #333; text-align: center; }
    input { width: 100%; padding: 10px; margin: 5px 0; box-sizing: border-box; border: 1px solid #ddd; border-radius: 4px; }
    button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; cursor: pointer; border-radius: 4px; font-size: 16px; margin-top: 10px; }
    button:hover { background: #45a049; }
    label { font-weight: bold; color: #555; }
  </style>
</head>
<body>
  <div class="container">
    <h2>ESP32 SMS Gateway</h2>
    <h3>WiFi Configuration</h3>
    <form action="/save" method="POST">
      <label>WiFi SSID:</label>
      <input type="text" name="ssid" required placeholder="Enter your WiFi name">
      <label>WiFi Password:</label>
      <input type="password" name="password" required placeholder="Enter your WiFi password">
      <button type="submit">Save & Restart</button>
    </form>
  </div>
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
    
    String successPage = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial; margin: 20px; text-align: center; }
        .success { color: #4CAF50; }
      </style>
    </head>
    <body>
      <h2 class="success">✓ Configuration Saved!</h2>
      <p>ESP32 will restart and connect to your network.</p>
      <p>Access the API at the new IP address.</p>
    </body>
    </html>
    )";
    
    server.send(200, "text/html", successPage);
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", 
      "<html><body><h2>Error</h2><p>Missing SSID or password</p></body></html>");
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
  Serial.println("Connect to: " + String(ap_ssid));
  Serial.println("Password: " + String(ap_password));
  
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
  Serial.println("Connecting to WiFi: " + ssid);
  isAPMode = false;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ Connected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup API routes
    server.on("/sms", HTTP_POST, handleSMS);
    server.on("/status", HTTP_GET, handleStatus);
    server.onNotFound([]() {
      server.send(404, "application/json", "{\"error\":\"Not found\"}");
    });
    
    server.begin();
    Serial.println("SMS API started");
    Serial.println("Endpoints:");
    Serial.println("  POST /sms - Send SMS");
    Serial.println("  GET /status - Check status");
  } else {
    Serial.println("\n✗ Failed to connect to WiFi");
    Serial.println("Starting AP mode for configuration...");
    startAPMode();
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD2, TXD2);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("ESP32 SMS Gateway Starting...");
  Serial.println("=================================\n");
  
  // Initialize SIM800L
  initSIM800L();
  
  // Load WiFi credentials
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
  
  // Check if credentials exist
  if (ssid.length() > 0) {
    startStationMode();
  } else {
    Serial.println("No WiFi credentials found");
    startAPMode();
  }
}

void loop() {
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
  
  // Forward serial communication for debugging
  while (Serial.available()) {
    Serial1.write(Serial.read());
  }
  
  delay(1);
}
