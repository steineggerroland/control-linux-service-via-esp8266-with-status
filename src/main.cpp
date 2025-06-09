#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

//////////////////////
// WLAN-Zugangsdaten
//////////////////////

#define WIFI_SSID     "DeinSSID"
#define WIFI_PASS     "DeinPasswort"

//////////////////////
// API-Konfiguration
//////////////////////

#define API_TOKEN     "geheimer-token-123"
#define BASE_URL      "http://raspberrypi.local:5000"

//////////////////////
// Hardware-Pins
//////////////////////

#define BUTTON_PIN    D6      // GPIO12
#define LED_PIN       D2      // GPIO4
#define NUM_LEDS      24

//////////////////////
// LED-Konfiguration
//////////////////////

#define COLOR_ORDER   GRB
#define LED_TYPE      WS2812B
CRGB leds[NUM_LEDS];

//////////////////////
// Statusvariablen
//////////////////////

bool serviceRunning = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long lastStatusCheck = 0;
const unsigned long statusInterval = 10000;

unsigned long buttonPressTime = 0;
bool buttonHandled = false;

//////////////////////
// WLAN verbinden
//////////////////////

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Verbinde mit WLAN");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWLAN verbunden, IP: " + WiFi.localIP().toString());
}

//////////////////////
// URL-Helfer
//////////////////////

String makeUrl(const char* endpoint) {
  return String(BASE_URL) + endpoint + "?token=" + API_TOKEN;
}

//////////////////////
// API-Aufrufe
//////////////////////

void sendStartCommand() {
  HTTPClient http;
  http.begin(makeUrl("/start"));
  int httpCode = http.GET();
  Serial.println("Start: HTTP " + String(httpCode));
  http.end();
}

void sendStopCommand() {
  HTTPClient http;
  http.begin(makeUrl("/stop"));
  int httpCode = http.GET();
  Serial.println("Stop: HTTP " + String(httpCode));
  http.end();
}

void checkServiceStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(makeUrl("/status"));
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      String status = doc["status"];
      serviceRunning = (status == "active");
      Serial.println("Service status: " + status);
    }
  } else {
    Serial.println("Status request failed: HTTP " + String(httpCode));
  }

  http.end();
}

//////////////////////
// LED-Anzeige
//////////////////////

void showStatus() {
  if (serviceRunning) {
    fill_solid(leds, NUM_LEDS, CRGB::Green);
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
  }
  FastLED.show();
}

void showBlink(CRGB color) {
  for (int i = 0; i < 2; i++) {
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    delay(150);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(150);
  }
}

//////////////////////
// Setup
//////////////////////

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear(); FastLED.show();

  connectWiFi();
  checkServiceStatus();
  showStatus();
}

//////////////////////
// Hauptloop
//////////////////////

void loop() {
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW) {
    if (buttonPressTime == 0) {
      buttonPressTime = millis();  // Startzeit merken
      buttonHandled = false;
    }

    unsigned long heldTime = millis() - buttonPressTime;

    // Kurzer Druck: Start (wenn Service aus)
    if (!buttonHandled && heldTime >= 100 && heldTime < 5000 && !serviceRunning) {
      sendStartCommand();
      showBlink(CRGB::Yellow);
      checkServiceStatus();
      showStatus();
      buttonHandled = true;
    }

    // Langer Druck: Stop (wenn Service läuft)
    if (!buttonHandled && heldTime >= 5000 && serviceRunning) {
      sendStopCommand();
      showBlink(CRGB::Red);
      checkServiceStatus();
      showStatus();
      buttonHandled = true;
    }

  } else {
    buttonPressTime = 0;  // Zurücksetzen, wenn losgelassen
    buttonHandled = false;
  }

  // Status regelmäßig prüfen
  if (millis() - lastStatusCheck > statusInterval) {
    checkServiceStatus();
    showStatus();
    lastStatusCheck = millis();
  }

  delay(10);
}
