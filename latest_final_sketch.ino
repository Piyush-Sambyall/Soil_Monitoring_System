#define BLYNK_TEMPLATE_ID "TMPL30WmZXpP8"
#define BLYNK_TEMPLATE_NAME "Soil Monitoring System"
#define BLYNK_AUTH_TOKEN "JlIfmN-eOhc1l-w4S6BwW_8mZ5lvavTq"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_VCNL4040.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// WiFi Config
char ssid[] = "hello2";
char pass[] = "hello1234";

String serverName = "http://10.160.56.28:5000/predict";

// OLED Screen Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensors 
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_VCNL4040 vcnl = Adafruit_VCNL4040();

// Pins
#define SOIL_PIN 34
#define GAS_PIN 35
#define BUZZER_PIN 25

// Soil calibration
int dry = 1300;
int wet = 1100;

void setup() {
  Serial.begin(115200);

  // WiFi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Blynk 
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  // I2C
  Wire.begin();

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Sensors
  sht4.begin();
  vcnl.begin();

  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  Blynk.run();

  // Sensor Readings
  int soilRaw = analogRead(SOIL_PIN);
  int soilPercent = map(soilRaw, dry, wet, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  int gas = analogRead(GAS_PIN);

  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp);

  uint16_t light = vcnl.getLux();

  // Send to BLYNK
  Blynk.virtualWrite(V0, soilPercent);
  Blynk.virtualWrite(V1, temp.temperature);
  Blynk.virtualWrite(V2, humidity.relative_humidity);
  Blynk.virtualWrite(V3, light);
  Blynk.virtualWrite(V4, gas);

  // Send to ML
  sendToML(soilPercent, temp.temperature, humidity.relative_humidity, light, gas);

  delay(5000);
}

// ML FUNCTION
void sendToML(int soil, float temp, float hum, int light, int gas) {

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"soil_moisture\":" + String(soil) + ",";
  json += "\"temperature\":" + String(temp) + ",";
  json += "\"humidity\":" + String(hum) + ",";
  json += "\"light\":" + String(light) + ",";
  json += "\"air_quality\":" + String(gas);
  json += "}";

  int code = http.POST(json);

  if (code > 0) {
    String response = http.getString();

    String prediction = "Unknown";

    if (response.indexOf("Healthy") != -1)
      prediction = "Healthy";
    else if (response.indexOf("Moderate") != -1)
      prediction = "Moderate";
    else
      prediction = "Critical";

    // Sent to BLYNK
    Blynk.virtualWrite(V5, prediction);

    // OLED Display
    display.clearDisplay();

    display.setCursor(0, 0);
    display.println("Soil: " + String(soil) + "%");

    display.setCursor(0, 10);
    display.println("Temp: " + String(temp));

    display.setCursor(0, 20);
    display.println("Hum: " + String(hum));

    display.setCursor(0, 30);
    display.println("Light: " + String(light));

    display.setCursor(0, 40);
    display.println("Air: " + String(gas));

    display.setCursor(0, 50);
    display.println("State: " + prediction);

    display.display();

    // Buzzer Alert
    if (prediction == "Critical") {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(5000);
      digitalWrite(BUZZER_PIN, LOW);
    }

  }

  http.end();
}
