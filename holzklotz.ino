#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define PIN_BUTTON D3
#define PIN_RGBLED D2

#define STASSID "ortenau.freifunk.net"
#define STAPSK  ""

// RGB LED
Adafruit_NeoPixel statusLed = Adafruit_NeoPixel(1, PIN_RGBLED, NEO_GRB + NEO_KHZ800);
int brightness = 4;
bool brighter = true;
unsigned long lastBrightnessChange = 0;
unsigned long brightnessDelay = 64;

// Button
int buttonState = LOW;
int thisButtonState = LOW;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Status
bool roomOpen = false;
unsigned long lastStatusUpdateTime = 0;
unsigned long statusUpdateDelay = 1 * 60 * 1000;

void setup() {
  pinMode(PIN_BUTTON, INPUT);
  thisButtonState = digitalRead(PIN_BUTTON);
  buttonState = thisButtonState;
  lastButtonState = thisButtonState;

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);

  statusLed.begin();

  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.println();
  

  WiFi.begin(STASSID, STAPSK);
  
  digitalWrite(BUILTIN_LED, HIGH);
}

void loop() {
    updateStatusFromApi();
    handleStatusLed();
    handleButton();
}

void handleStatusLed() {
  if ((millis() - lastBrightnessChange) > brightnessDelay) {
    brightness = brighter ? brightness + 1 : brightness - 1;

    if (brightness == 64) {
      brighter = false; 
    } else if (brightness == 4) {
      brighter = true;
    }

    lastBrightnessChange = millis();
  }

  if (WiFi.status() == WL_CONNECTED && lastStatusUpdateTime > 0) {
    if (roomOpen) {
      statusLed.setPixelColor(0, 0, brightness, 0);
    } else {
      statusLed.setPixelColor(0, brightness, 0, 0);
    }
  } else {
    statusLed.setPixelColor(0, 0, 0, brightness);
  }
  statusLed.show();
}

void handleButton() {
  thisButtonState = digitalRead(PIN_BUTTON);
  if (thisButtonState != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (thisButtonState != buttonState) {
      buttonState = thisButtonState;

      if (buttonState == HIGH && WiFi.status() == WL_CONNECTED && lastStatusUpdateTime > 0) {
        roomOpen = !roomOpen;
        sendStatusToApi();
      }
    }
  }

  lastButtonState = thisButtonState;
}

void sendStatusToApi() {
    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    http.begin(client, "http://api.section77.de/sensors/people_now_present/");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    Serial.print("[HTTP] PUT...\n");
    int httpCode = http.PUT(roomOpen ? "value=1" : "value=0");

    if (httpCode > 0) {
      Serial.printf("[HTTP] PUT... code: %d\n", httpCode);
    } else {
      Serial.printf("[HTTP] PUT... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
}

void updateStatusFromApi() {
  if (WiFi.status() == WL_CONNECTED && ((millis() - lastStatusUpdateTime) > statusUpdateDelay || lastStatusUpdateTime == 0)) {
    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    http.begin(client, "http://api.section77.de/");

    Serial.print("[HTTP] GET...\n");
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      String payload = http.getString();
      if (payload.indexOf("\"state\":{\"open\":") > 0) {
        roomOpen = payload.indexOf("\"state\":{\"open\":true") > 0;
        Serial.printf("[HTTP] GET... newState: %s\n", roomOpen ? "true" : "false");
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

    lastStatusUpdateTime = millis();
  }
}