#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>

const int RELAY_PIN = D0;
const int BUTTON_PIN = 0;

const int PULSE_DURATION_MS = 50;
const int DEBOUNCE_DELAY_MS = 50;

const int RELAY_ON = HIGH;
const int RELAY_OFF = LOW;

const char *ssid = "SEEED-MKT";
const char *password = "edgemaker2023";

int lastButtonState = HIGH;
int currentStableState = HIGH;

unsigned long lastDebounceTime = 0;
bool isRelayActive = false;
unsigned long relayStartTime = 0;

String serialCommand;
WebServer server(80);

/**
 * @brief Generate a relay pulse to simulate coin insertion (Non-blocking).
 *
 * @param source The source that triggered the pulse (e.g., "serial", "button", "HTTP").
 */
void triggerCoinPulse(const char* source) {
  if (isRelayActive) {
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
void handleRelay() {
  if (isRelayActive) {
    if (millis() - relayStartTime >= PULSE_DURATION_MS) {
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
void handleSerialInput() {
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();

    if (incomingChar == '\n') {
      serialCommand.trim();

      if (serialCommand.equals("TRIGGER")) {
        triggerCoinPulse("serial command");
      } else if (serialCommand.length() > 0) {
        Serial.print("Unknown command: ");
        Serial.println(serialCommand);
      }

      serialCommand = "";
    } else {
      if (isGraph(incomingChar)) {
        serialCommand += incomingChar;
      }
    }
  }
}

/**
 * @brief Handle push button input with debounce (Non-blocking).
 */
void handleButtonInput() {
  int reading = digitalRead(BUTTON_PIN);

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != currentStableState) {
      currentStableState = reading;

      // only trigger if the new button state is LOW (pressed)
      if (currentStableState == LOW) {
        triggerCoinPulse("button press");
      }
    }
  }

  lastButtonState = reading;
}

/**
 * @brief HTTP handler for the root path.
 *
 * Serves "/game.html" from LittleFS as the main game page.
 */
void handleRoot() {
  if (!LittleFS.exists("/game.html")) {
    Serial.println("game.html not found in LittleFS");
    server.send(500, "text/plain", "game.html not found");
    return;
  }

  File file = LittleFS.open("/game.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open game.html");
    return;
  }

  server.streamFile(file, "text/html; charset=utf-8");
  file.close();
}

/**
 * @brief HTTP handler for "/insert_coin".
 *
 * Triggers a coin pulse and responds with a JSON status.
 */
void handleInsertCoin() {
  Serial.println("HTTP /insert_coin called");
  triggerCoinPulse("HTTP request");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

/**
 * @brief HTTP 404 handler.
 */
void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  serialCommand.reserve(32);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, RELAY_OFF);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
  } else {
    Serial.println("LittleFS mounted successfully.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("claw-service")) {
    Serial.println("MDNS responder started");
  }

  // Set HTTP routing
  server.on("/", HTTP_GET, handleRoot);
  server.on("/insert_coin", HTTP_GET, handleInsertCoin);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  Serial.println("Pulse Controller Initialized.");
  Serial.println("Send 'TRIGGER' or press the built-in button.");
  Serial.println("Open http://claw-service.local/ or the IP address above.");
}

void loop() {
  server.handleClient();
  handleSerialInput();
  handleButtonInput();
  handleRelay();
}
