#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define RELE_PIN 14
#define TRIG 33
#define ECHO 32

float H_TANQUE = 9.5;

const char* ssid = "NOMBRE_WIFI";
const char* password = "CONTRASEÑA_WIFI";


String serverUrl = "http://SERVER_RENDER/api/update";

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool bomba = false;

float readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);
  return duration * 0.034 / 2.0;
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, LOW);

  Wire.begin(25, 13);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Conectando...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
  lcd.clear();
  lcd.print("WiFi Conectado");
  delay(1500);
  lcd.clear();
}

void loop() {   
  float D_SENSOR = readDistance() - 2;
  float nivel_cm = H_TANQUE - D_SENSOR;

  if (nivel_cm < 0) nivel_cm = 0;
  if (nivel_cm > H_TANQUE) nivel_cm = H_TANQUE;

  int capacidad = (nivel_cm * 100) / H_TANQUE;

  String nivel;
  if (capacidad >= 75) {
    nivel = "ALTO";
  } 
  else if (capacidad >= 50) {
    nivel = "MEDIO";
  } 
  else {
    nivel = "BAJO";
  }

  if (!bomba && capacidad <= 25) {
    bomba = true;
  }

  if (bomba && capacidad >= 90) {
    bomba = false;
  }

  digitalWrite(RELE_PIN, bomba ? HIGH : LOW);

  lcd.setCursor(0, 0);
  lcd.print("Nivel: ");
  lcd.print(nivel_cm, 2);
  lcd.print(" cm   ");

  lcd.setCursor(0, 1);
  lcd.print(capacidad);
  lcd.print("% ");
  lcd.print(nivel);
  lcd.print(" B:");
  lcd.print(bomba ? "ON " : "OFF");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    String json = "{"
      "\"nivel_cm\":" + String(nivel_cm, 2) +
      ",\"nivel\":\"" + nivel +
      "\",\"bomba\":" + String(bomba) +
      ",\"capacidad\":" + String(capacidad) +
    "}";

    Serial.println("Enviando JSON:");
    Serial.println(json);

    int httpResponseCode = http.POST(json);
    Serial.print("HTTP Response: ");
    Serial.println(httpResponseCode);
    http.end();
  }

  delay(1000);
}
