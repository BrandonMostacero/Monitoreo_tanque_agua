#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define RELE_PIN 14
#define TRIG 33
#define ECHO 32

const char* ssid = "NOMBRE_WIFI";
const char* password = "CLAVE_WIFI";

String serverUpdateUrl = "https://SERVER_RENDER.com/api/update";
String serverConfigUrl = "https://SERVER_RENDER.com/api/control";

LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences prefs;

float OFFSET_SENSOR = 2.0;
float H_TANQUE = 0.0;
bool bomba = false;
bool calibrado = false;

float readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);
  if (duration == 0) return -1;

  return duration * 0.0343 / 2.0;
}

float calibrarTanque() {
  float suma = 0;
  int muestras = 10;

  for (int i = 0; i < muestras; i++) {
    float d = readDistance();
    if (d > 0) suma += d;
    delay(150);
  }
  return suma / muestras;
}

void guardarAltura(float altura) {
  prefs.begin("tanque", false);
  prefs.putFloat("altura", altura);
  prefs.end();

  H_TANQUE = altura;
  calibrado = true;
}

void verificarCalibracionServidor() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(serverConfigUrl);

  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);

    bool calibrar = doc["calibrar"];

    if (calibrar) {
      lcd.clear();
      lcd.print("Calibrando...");
      Serial.println("CALIBRACION REMOTA");

      float altura = calibrarTanque() - OFFSET_SENSOR;
      guardarAltura(altura);

      lcd.clear();
      lcd.print("Altura: ");
      lcd.print(altura);
      lcd.print(" cm");
      delay(1500);
    }
  }

  http.end();
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

  prefs.begin("tanque", true);
  H_TANQUE = prefs.getFloat("altura", 0.0);
  prefs.end();

  calibrado = (H_TANQUE > 0);

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
  verificarCalibracionServidor();

  if (!calibrado) {
    lcd.setCursor(0, 0);
    lcd.print("No calibrado ");
    delay(1000);
    return;
  }

  float D_SENSOR = readDistance() - OFFSET_SENSOR;
  if (D_SENSOR < 0) D_SENSOR = 0;

  float nivel_cm = H_TANQUE - D_SENSOR;
  nivel_cm = constrain(nivel_cm, 0, H_TANQUE);

  int capacidad = (nivel_cm * 100) / H_TANQUE;

  String nivel;
  if (capacidad >= 75) nivel = "ALTO";
  else if (capacidad >= 50) nivel = "MEDIO";
  else nivel = "BAJO";

  if (!bomba && capacidad <= 25) bomba = true;
  if (bomba && capacidad >= 90) bomba = false;

  digitalWrite(RELE_PIN, bomba ? HIGH : LOW);

  lcd.setCursor(0, 0);
  lcd.print("Nivel: ");
  lcd.print(nivel_cm, 2);
  lcd.print(" cm      ");

  lcd.setCursor(0, 1);
  lcd.print(capacidad);
  lcd.print("% ");
  lcd.print("B:");
  lcd.print(bomba ? "ON      " : "OFF      ");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUpdateUrl);
    http.addHeader("Content-Type", "application/json");

    String json = "{"
      "\"nivel_cm\":" + String(nivel_cm, 2) +
      ",\"nivel\":\"" + nivel +
      "\",\"bomba\":" + String(bomba) +
      ",\"capacidad\":" + String(capacidad) +
    "}";

    http.POST(json);
    http.end();
  }

  delay(1500);
}
