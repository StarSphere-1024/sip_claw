#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

const int RELAY_PIN = D0;
const int BUTTON_PIN = 0;

const int PULSE_DURATION_MS = 50;
const int DEBOUNCE_DELAY_MS = 50;

const int RELAY_ON = HIGH;
const int RELAY_OFF = LOW;

String ssid;
String wifiPassword;

// Cache for HTML files to reduce I/O
String gameHtmlCache;
String adminHtmlCache;

int lastButtonState = HIGH;
int currentStableState = HIGH;

unsigned long lastDebounceTime = 0;
bool isRelayActive = false;
unsigned long relayStartTime = 0;

String serialCommand;
WebServer server(80);
Preferences preferences;

// Game Parameters
int decaySpeed = 45;
int tapPower = 5;
int gameDuration = 10;
String adminPassword = "admin";

/**
 * @brief Update an integer preference if the argument exists and is valid.
 * 
 * @param argName The name of the HTTP argument.
 * @param var Reference to the variable to update.
 * @param prefKey The key in Preferences.
 * @param minVal Minimum valid value (inclusive).
 */
void updateIntPref(const String& argName, int& var, const char* prefKey, int minVal) {
    if (server.hasArg(argName)) {
        int val = server.arg(argName).toInt();
        if (val >= minVal) {
            var = val;
            preferences.putInt(prefKey, var);
        }
    }
}

/**
 * @brief Update a string preference if the argument exists and is not empty.
 * 
 * @param argName The name of the HTTP argument.
 * @param var Reference to the variable to update.
 * @param prefKey The key in Preferences.
 */
void updateStringPref(const String& argName, String& var, const char* prefKey) {
    if (server.hasArg(argName) && server.arg(argName).length() > 0) {
        var = server.arg(argName);
        preferences.putString(prefKey, var);
    }
}

/**
 * @brief Generate a relay pulse to simulate coin insertion (Non-blocking).
 *
 * @param source The source that triggered the pulse (e.g., "serial", "button", "HTTP").
 */
void triggerCoinPulse(const char *source)
{
    if (isRelayActive)
    {
        Serial.println("Pulse already active, ignoring trigger.");
        return;
    }
    Serial.print("Coin pulse triggered by: ");
    Serial.println(source);

    digitalWrite(RELAY_PIN, RELAY_ON);
    isRelayActive = true;
    relayStartTime = millis();
}

/**
 * @brief Handle relay timing logic. Must be called in loop().
 */
void handleRelay()
{
    if (isRelayActive)
    {
        if (millis() - relayStartTime >= PULSE_DURATION_MS)
        {
            digitalWrite(RELAY_PIN, RELAY_OFF);
            isRelayActive = false;
            Serial.println("Pulse finished.");
        }
    }
}

/**
 * @brief Handle incoming serial commands.
 *
 * Supported commands:
 *  - "TRIGGER" : trigger a coin pulse.
 */
void handleSerialInput()
{
    if (Serial.available() > 0)
    {
        char incomingChar = Serial.read();

        if (incomingChar == '\n')
        {
            serialCommand.trim();

            if (serialCommand.equals("TRIGGER"))
            {
                triggerCoinPulse("serial command");
            }
            else if (serialCommand.length() > 0)
            {
                Serial.print("Unknown command: ");
                Serial.println(serialCommand);
            }

            serialCommand = "";
        }
        else
        {
            if (isGraph(incomingChar))
            {
                serialCommand += incomingChar;
            }
        }
    }
}

/**
 * @brief Handle push button input with debounce (Non-blocking).
 */
void handleButtonInput()
{
    int reading = digitalRead(BUTTON_PIN);

    // If the switch changed, due to noise or pressing:
    if (reading != lastButtonState)
    {
        // reset the debouncing timer
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS)
    {
        // whatever the reading is at, it's been there for longer than the debounce
        // delay, so take it as the actual current state:

        // if the button state has changed:
        if (reading != currentStableState)
        {
            currentStableState = reading;

            // only trigger if the new button state is LOW (pressed)
            if (currentStableState == LOW)
            {
                triggerCoinPulse("button press");
            }
        }
    }

    lastButtonState = reading;
}

/**
 * @brief HTTP handler for the root path.
 *
 * Serves "/game.html" from memory cache.
 */
void handleRoot()
{
    if (gameHtmlCache.length() == 0)
    {
        server.send(500, "text/plain", "game.html not loaded");
        return;
    }
    server.send(200, "text/html", gameHtmlCache);
}

/**
 * @brief HTTP handler for "/insert_coin".
 *
 * Triggers a coin pulse and responds with a JSON status.
 */
void handleInsertCoin()
{
    Serial.println("HTTP /insert_coin called");
    triggerCoinPulse("HTTP request");
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

/**
 * @brief HTTP 404 handler.
 */
void handleNotFound()
{
    server.send(404, "text/plain", "Not found");
}

void handleAdmin()
{
    if (adminHtmlCache.length() == 0)
    {
        server.send(500, "text/plain", "admin.html not loaded");
        return;
    }
    server.send(200, "text/html", adminHtmlCache);
}

void handleGetConfig()
{
    JsonDocument doc;
    doc["decaySpeed"] = decaySpeed;
    doc["tapPower"] = tapPower;
    doc["gameDuration"] = gameDuration;
    doc["ssid"] = ssid;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

/**
 * @brief Verify admin password for login.
 */
void handleAdminLogin()
{
    if (!server.hasArg("password"))
    {
        server.send(400, "application/json", "{\"success\":false, \"message\":\"Missing password\"}");
        return;
    }
    if (server.arg("password") == adminPassword)
    {
        server.send(200, "application/json", "{\"success\":true}");
    }
    else
    {
        server.send(200, "application/json", "{\"success\":false, \"message\":\"Invalid password\"}");
    }
}

/**
 * @brief Handle configuration save requests.
 * 
 * Validates admin password and updates game/network settings.
 */
void handleSaveConfig()
{
    if (!server.hasArg("password"))
    {
        server.send(400, "application/json", "{\"success\":false, \"message\":\"Missing password\"}");
        return;
    }

    if (server.arg("password") != adminPassword)
    {
        server.send(200, "application/json", "{\"success\":false, \"message\":\"Invalid password\"}");
        return;
    }

    updateIntPref("decaySpeed", decaySpeed, "decaySpeed", 0);
    updateIntPref("tapPower", tapPower, "tapPower", 1);
    updateIntPref("gameDuration", gameDuration, "gameDuration", 1);
    
    updateStringPref("newPassword", adminPassword, "adminPassword");
    updateStringPref("ssid", ssid, "ssid");
    updateStringPref("wifiPassword", wifiPassword, "wifiPassword");

    server.send(200, "application/json", "{\"success\":true}");
}

void setup()
{
    Serial.begin(115200);
    serialCommand.reserve(32);

    preferences.begin("game-config", false);

    // Load config from NVS, use defaults if not found
    decaySpeed = preferences.getInt("decaySpeed", 45);
    tapPower = preferences.getInt("tapPower", 5);
    gameDuration = preferences.getInt("gameDuration", 10);
    adminPassword = preferences.getString("adminPassword", "admin");
    ssid = preferences.getString("ssid", "SEEED-MKT");
    wifiPassword = preferences.getString("wifiPassword", "edgemaker2023");

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    digitalWrite(RELAY_PIN, RELAY_OFF);

    if (!LittleFS.begin(true))
    {
        Serial.println("LittleFS mount failed!");
    }
    else
    {
        Serial.println("LittleFS mounted successfully.");
        // Preload HTML files
        if (LittleFS.exists("/game.html")) {
            File file = LittleFS.open("/game.html", "r");
            gameHtmlCache = file.readString();
            file.close();
        }
        if (LittleFS.exists("/admin.html")) {
            File file = LittleFS.open("/admin.html", "r");
            adminHtmlCache = file.readString();
            file.close();
        }
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), wifiPassword.c_str());
    Serial.println("");

    // Wait for connection with timeout (30s)
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("WiFi Failed! Starting AP Mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Claw-Game-Fallback", "12345678");
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    }

    if (MDNS.begin("claw-service"))
    {
        Serial.println("MDNS responder started");
    }

    // Set HTTP routing
    server.on("/", HTTP_GET, handleRoot);
    server.on("/insert_coin", HTTP_GET, handleInsertCoin);
    server.on("/admin", HTTP_GET, handleAdmin);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/admin/verify", HTTP_POST, handleAdminLogin);
    server.on("/api/admin/save", HTTP_POST, handleSaveConfig);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");

    Serial.println("Pulse Controller Initialized.");
    Serial.println("Send 'TRIGGER' or press the built-in button.");
    Serial.println("Open http://claw-service.local/ or the IP address above.");
    Serial.println("Admin Page: http://claw-service.local/admin");
    Serial.print("Admin Password: ");
    Serial.println(adminPassword);
}

void loop()
{

    // Auto-reconnect WiFi if lost (only in STA mode)
    if (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED)
    {
        static unsigned long lastReconnectAttempt = 0;
        if (millis() - lastReconnectAttempt > 5000)
        {
            Serial.println("WiFi lost, trying to reconnect...");
            WiFi.reconnect();
            lastReconnectAttempt = millis();
        }
    }

    server.handleClient();
    handleSerialInput();
    handleButtonInput();
    handleRelay();
}
