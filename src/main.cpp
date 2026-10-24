
/*
  EcoRack AI - ESP32 + DHT22 + SSD1306 OLED
  Three LEDs: GREEN, YELLOW, RED
  Fan + buzzer + live website sensor data

  Temperature:
  GREEN  = below 28°C
  YELLOW = 28°C to below 30°C
  RED    = 30°C or above

  Wiring:
  DHT22 DATA -> GPIO 4
  OLED SDA   -> GPIO 21
  OLED SCL   -> GPIO 22
  Green LED  -> GPIO 18 through 220-330 ohm resistor
  Yellow LED -> GPIO 19 through 220-330 ohm resistor
  Red LED    -> GPIO 23 through 220-330 ohm resistor
  Buzzer     -> GPIO 26 (use a suitable driver if required)
  Fan driver -> GPIO 25

  Install these Arduino libraries:
  - DHT sensor library
  - Adafruit Unified Sensor
  - Adafruit GFX Library
  - Adafruit SSD1306
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// -------------------- SENSOR CONFIGURATION --------------------

#define NUM_SENSORS 1
const uint8_t DHT_PINS[4] = {4, 16, 17, 27};

#define DHT_TYPE DHT22

// -------------------- OUTPUT PINS --------------------

#define LED_GREEN 18
#define LED_YELLOW 19
#define LED_RED 23
#define BUZZER_PIN 13
#define FAN_PIN 25

// -------------------- WIFI CONFIGURATION --------------------

// Use your actual Wi-Fi name and password.
const char *WIFI_SSID = "saniza";
const char *WIFI_PASSWORD = "12345678";

// -------------------- TEMPERATURE SETTINGS --------------------

float coolBelowC = 28.0;
float highFromC = 30.0;

// Turn the fan off at or below 28°C after it has started.
const float FAN_HYSTERESIS_C = 2.0;

// -------------------- OBJECTS --------------------

Preferences preferences;
DHT *dht[4];

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
WebServer server(80);

// -------------------- SENSOR DATA --------------------

float t[4];
float h[4];

bool fanOn = false;
bool alarmOn = false;
bool oledOk = false;

const char *state = "starting";

int hot = -1;
unsigned long lastRead = 0;

// -------------------- TEMPERATURE / LED CONTROL --------------------

void update()
{
  hot = -1;

  // Find the rack with the highest valid temperature.
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    if (!isnan(t[i]) && (hot < 0 || t[i] > t[hot]))
    {
      hot = i;
    }
  }

  // Sensor failure: indicate an error using the red LED.
  if (hot < 0)
  {
    state = "sensor_error";
    alarmOn = false;
    fanOn = true;

    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(FAN_PIN, HIGH);

    return;
  }

  float T = t[hot];

  // Switch all LEDs off before selecting the correct one.
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  // GREEN: temperature below 28°C.
  if (T < coolBelowC)
  {
    state = "cool";
    digitalWrite(LED_GREEN, HIGH);
    alarmOn = false;
  }
  // YELLOW: temperature from 28°C to below 30°C.
  else if (T < highFromC)
  {
    state = "normal";
    digitalWrite(LED_YELLOW, HIGH);
    alarmOn = false;
  }
  // RED: temperature at or above 30°C.
  else
  {
    state = "high";
    digitalWrite(LED_RED, HIGH);
    alarmOn = true;
  }

  // Fan control with hysteresis.
  if (T >= highFromC)
  {
    fanOn = true;
  }
  else if (T <= highFromC - FAN_HYSTERESIS_C)
  {
    fanOn = false;
  }

  digitalWrite(FAN_PIN, fanOn ? HIGH : LOW);
}

// -------------------- OLED DISPLAY --------------------

void drawOled()
{
  if (!oledOk)
  {
    return;
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);

  if (hot >= 0)
  {
    oled.printf("EcoRack AI Rack %d", hot + 1);
  }
  else
  {
    oled.print("EcoRack AI");
  }

  oled.setTextSize(2);
  oled.setCursor(0, 14);

  if (hot >= 0)
  {
    oled.printf("%.1f C", t[hot]);
  }
  else
  {
    oled.print("SENSOR?");
  }

  oled.setTextSize(1);
  oled.setCursor(0, 36);

  if (hot >= 0 && !isnan(h[hot]))
  {
    oled.printf("Humidity %.1f%%", h[hot]);
  }
  else
  {
    oled.print("Humidity: --");
  }

  oled.setCursor(0, 46);
  oled.printf("State: %s", state);

  oled.setCursor(0, 56);
  oled.printf("FAN:%s BUZZER:%s",
              fanOn ? "ON" : "OFF",
              alarmOn ? "ON" : "OFF");

  oled.display();
}

// -------------------- CORS FOR WEBSITE --------------------

void corsHeaders()
{
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions()
{
  corsHeaders();
  server.send(204);
}

// -------------------- LIVE SENSOR DATA API --------------------

void handleData()
{
  corsHeaders();

  String j = "{\"racks\":[";

  for (int i = 0; i < NUM_SENSORS; i++)
  {
    if (i > 0)
    {
      j += ",";
    }

    j += "{\"id\":" + String(i + 1);
    j += ",\"temperature_c\":" +
         String(isnan(t[i]) ? -999.0 : t[i], 1);
    j += ",\"humidity_pct\":" +
         String(isnan(h[i]) ? -1.0 : h[i], 1);
    j += "}";
  }

  j += "],\"temperature_c\":";
  j += String(hot >= 0 ? t[hot] : -999.0, 1);

  j += ",\"humidity_pct\":";
  j += String(hot >= 0 ? h[hot] : -1.0, 1);

  j += ",\"status\":\"";
  j += state;
  j += "\",\"fan_on\":";
  j += fanOn ? "true" : "false";

  j += ",\"buzzer_on\":";
  j += alarmOn ? "true" : "false";

  j += ",\"cool_below_c\":";
  j += String(coolBelowC, 1);

  j += ",\"high_from_c\":";
  j += String(highFromC, 1);

  j += "}";

  server.send(200, "application/json", j);
}

// -------------------- WEBSITE THRESHOLD API --------------------

void handleThreshold()
{
  corsHeaders();

  if (!server.hasArg("cool_below_c") ||
      !server.hasArg("high_from_c"))
  {
    server.send(
        400,
        "application/json",
        "{\"ok\":false,\"error\":\"Provide cool_below_c and high_from_c\"}");
    return;
  }

  float newCool = server.arg("cool_below_c").toFloat();
  float newHigh = server.arg("high_from_c").toFloat();

  if (newCool < 10.0 ||
      newCool > 45.0 ||
      newHigh < 15.0 ||
      newHigh > 60.0 ||
      newHigh <= newCool)
  {
    server.send(
        400,
        "application/json",
        "{\"ok\":false,\"error\":\"Invalid temperature thresholds\"}");
    return;
  }

  coolBelowC = newCool;
  highFromC = newHigh;

  preferences.putFloat("cool", coolBelowC);
  preferences.putFloat("high", highFromC);

  update();
  drawOled();

  String response = "{\"ok\":true,\"cool_below_c\":";
  response += String(coolBelowC, 1);
  response += ",\"high_from_c\":";
  response += String(highFromC, 1);
  response += "}";

  server.send(200, "application/json", response);
}

// -------------------- SETUP --------------------

void setup()
{
  Serial.begin(115200);

  // Use fixed required thresholds at every boot.
  // This prevents old saved values from changing the LED limits.
  preferences.begin("ecorack", false);

  coolBelowC = 28.0;
  highFromC = 30.0;

  preferences.putFloat("cool", coolBelowC);
  preferences.putFloat("high", highFromC);

  // Configure output pins.
  int outs[] = {
      LED_GREEN,
      LED_YELLOW,
      LED_RED,
      BUZZER_PIN,
      FAN_PIN};

  for (int p : outs)
  {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

  // Initialize DHT22 sensors.
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    dht[i] = new DHT(DHT_PINS[i], DHT_TYPE);
    dht[i]->begin();

    t[i] = NAN;
    h[i] = NAN;
  }

  // Initialize OLED.
  Wire.begin(21, 22);

  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  if (oledOk)
  {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("EcoRack AI");
    oled.println("Connecting WiFi...");
    oled.display();
  }

  // Connect to Wi-Fi.
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi...");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 15000)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println();
    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    // Register website API endpoints.
    server.on("/data", HTTP_GET, handleData);

    server.on("/threshold", HTTP_OPTIONS, handleOptions);
    server.on("/threshold", HTTP_POST, handleThreshold);

    server.on("/", HTTP_GET, []()
              { server.send(
                    200,
                    "text/plain",
                    "EcoRack AI is online. Open /data for live sensor JSON."); });

    server.onNotFound([]()
                      {
      corsHeaders();
      server.send(
          404,
          "application/json",
          "{\"ok\":false,\"error\":\"Endpoint not found\"}"
      ); });

    server.begin();

    Serial.println("HTTP server started.");
    Serial.print("Test URL: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/data");

    if (oledOk)
    {
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(0, 0);
      oled.println("WiFi connected");
      oled.println("ESP32 IP:");
      oled.println(WiFi.localIP());
      oled.display();

      delay(2000);
    }
  }
  else
  {
    Serial.println();
    Serial.println("Wi-Fi connection failed.");
    Serial.println("Check Wi-Fi credentials and network.");
  }
}

// -------------------- MAIN LOOP --------------------

void loop()
{
  // DHT22 should not be read more frequently than every 2 seconds.
  if (millis() - lastRead >= 2000)
  {
    lastRead = millis();

    for (int i = 0; i < NUM_SENSORS; i++)
    {
      t[i] = dht[i]->readTemperature();
      h[i] = dht[i]->readHumidity();
    }

    update();
    drawOled();

    if (hot >= 0)
    {
      Serial.printf(
          "Temperature: %.1f C | Humidity: %.1f%% | State: %s | Fan: %s\n",
          t[hot],
          h[hot],
          state,
          fanOn ? "ON" : "OFF");
    }
    else
    {
      Serial.println("DHT22 sensor error: check wiring.");
    }
  }

  // Buzzer beeps while temperature is at or above the high threshold.
  digitalWrite(
      BUZZER_PIN,
      alarmOn && ((millis() / 300) % 2 == 0) ? HIGH : LOW);

  // Keep the HTTP server responsive to website requests.
  if (WiFi.status() == WL_CONNECTED)
  {
    server.handleClient();
  }
}
