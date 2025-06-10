#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <ArduinoOTA.h>

//////////////////////
// WLAN-Zugangsdaten
//////////////////////

#define WIFI_SSID     "SSID"
#define WIFI_PASS     "PW"

//////////////////////
// API-Konfiguration
//////////////////////

#define API_TOKEN     "geheimer API Key"
#define BASE_URL      "http://service.local:5000"

//////////////////////
// Hardware-Pins
//////////////////////

#define BUTTON_PIN    D6      // GPIO12
#define LED_PIN       D2      // GPIO4
#define NUM_LEDS      25

//////////////////////
// LED-Konfiguration
//////////////////////

#define COLOR_ORDER   GRB
#define LED_TYPE      WS2812B
CRGB leds[NUM_LEDS];
// Fire animation on stopped service
#define COOLING  50
#define SPARKING 50
// CRGBPalette16 gPal = HeatColors_p;
CRGBPalette16 gPal = CRGBPalette16(CRGB::Black, CRGB::DarkRed, CRGB::Red, CRGB::OrangeRed);
#define BRIGHTNESS 200
#define FRAMES_PER_SECOND 60

//////////////////////
// Statusvariablen
//////////////////////

bool serviceRunning = false;
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
// OTA-Setup
//////////////////////

void setupOta() {
  ArduinoOTA.setHostname("d1mini-steuerung");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Ende");
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int p = map(progress, 0, total, 0, NUM_LEDS);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < p; i++) leds[i] = CRGB::Blue;
    FastLED.show();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler [%u]\n", error);
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
  });

  ArduinoOTA.begin();
  Serial.println("OTA bereit");
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
  WiFiClient client;
  HTTPClient http;
  http.begin(client, makeUrl("/start"));
  int httpCode = http.GET();
  Serial.println("Start: HTTP " + String(httpCode));
  http.end();
}

void sendStopCommand() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, makeUrl("/stop"));
  int httpCode = http.GET();
  Serial.println("Stop: HTTP " + String(httpCode));
  http.end();
}

void checkServiceStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  http.begin(client, makeUrl("/status"));
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
// LED-Animationen
//////////////////////

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

void showStartingAnimation() {
  const unsigned long duration = 40000;
  const unsigned long interval = duration / NUM_LEDS;

  FastLED.clear();
  FastLED.show();

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Green;
    FastLED.show();
    delay(interval);
  }
}

void animateRunningService() {
  static uint8_t brightness = 150;
  static uint8_t minimum = 80;
  static uint8_t maximum = 255;
  static int8_t direction = 10;
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate > 200) {
    brightness += direction;
    brightness = max(minimum, min(maximum, brightness));
    if (brightness >= maximum || brightness <= minimum) direction = -direction;

    fill_solid(leds, NUM_LEDS, CRGB(0, brightness, 0));
    FastLED.show();
    lastUpdate = millis();
    
  }
}

void animateStoppedService() {
  static uint8_t heat[NUM_LEDS];

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 200) {
    // Step 1. Cool down every cell a little
    for (int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }

    // Step 2. Heat from each cell drifts up and diffuses a little
    for (int k = NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3. Randomly ignite new 'sparks' near the bottom
    if (random8() < SPARKING) {
      int y = random8(7);
      heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4. Map heat cells to LED colors
    for (int j = 0; j < NUM_LEDS; j++) {
      uint8_t colorindex = scale8(heat[j], 240);
      CRGB color = ColorFromPalette(gPal, colorindex);
      leds[j] = color;
    }

    lastUpdate = millis();
  }

  FastLED.show();
}

//////////////////////
// Setup
//////////////////////

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(); FastLED.show();

  connectWiFi();
  setupOta();
  checkServiceStatus();
}

//////////////////////
// Hauptloop
//////////////////////

void loop() {
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW) {
    if (buttonPressTime == 0) {
      buttonPressTime = millis();
      buttonHandled = false;
    }

    unsigned long heldTime = millis() - buttonPressTime;

    // Kurzer Druck: Start (wenn Service aus)
    if (!buttonHandled && heldTime >= 100 && heldTime < 5000 && !serviceRunning) {
      sendStartCommand();
      showStartingAnimation();
      checkServiceStatus();
      buttonHandled = true;
    }

    // Langer Druck: Stop (wenn Service läuft)
    if (!buttonHandled && heldTime >= 5000 && serviceRunning) {
      sendStopCommand();
      showBlink(CRGB::Red);
      checkServiceStatus();
      buttonHandled = true;
    }

  } else {
    buttonPressTime = 0;
    buttonHandled = false;
  }

  // Regelmäßiger Status-Check
  if (millis() - lastStatusCheck > statusInterval) {
    checkServiceStatus();
    lastStatusCheck = millis();
  }

  // Hintergrundanimation
  if (buttonPressTime == 0 && !buttonHandled) {
    if (serviceRunning) {
      animateRunningService();
    } else {
      animateStoppedService();
    }
  }

  ArduinoOTA.handle();
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
