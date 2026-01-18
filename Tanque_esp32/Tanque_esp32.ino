// Importacion de las librerías necesarias
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Definición de pines del relé y del sensor
#define RELE_PIN 14
#define TRIG 33
#define ECHO 32

// Variables con el ssid y contraseña del WiFi a conectarse
const char* ssid = "NOMBRE_WIFI";
const char* password = "CLAVE_WIFI";

// Variables con los URL para POST y GET del esp32
String serverUpdateUrl = "https://SERVER_RENDER.com/api/update";
String serverConfigUrl = "https://SERVER_RENDER.com/api/control";

LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences prefs;

float OFFSET_SENSOR = 2.0; // Distancia (en cm) paa que no alcance mas de 2cm la altura del agua
float H_TANQUE = 0.0;
bool bomba = false;
bool calibrado = false;

// Función de la lectura del sensor
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

// Funcion para calibrar tanque con lecturas promedio
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

// Funcion para guardar la altura del tanque
void guardarAltura(float altura) {
  prefs.begin("tanque", false);
  prefs.putFloat("altura", altura);
  prefs.end();

  H_TANQUE = altura;
  calibrado = true;
}

// Verifica si el servidor envia la solicitud para calibrar
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

      // Altura real del tanque es la distancia medida por el sensor - 2 cm 
      // Para proteger al sensor por desbordamiento
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

  // Mientras no se conecte a WiFi no se ejecutará el resto de código
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
  // Verifica la solicitud en cada vuelta
  verificarCalibracionServidor();

  // Si aun no se ha calibrado la altura, se mostrará en pantalla
  if (!calibrado) {
    lcd.setCursor(0, 0);
    lcd.print("No calibrado ");
    delay(1000);
    return;
  }

  // Distancia medida por el sensor
  float D_SENSOR = readDistance();
  if (D_SENSOR < 0) D_SENSOR = 0;

  // Altura del agua
  float nivel_cm = H_TANQUE - (D_SENSOR - OFFSET_SENSOR);

  // Mantiene a la altura del agua en valores entre 0 y H_TANQUE
  nivel_cm = constrain(nivel_cm, 0, H_TANQUE);

  // Capacidad del tanque en porcentaje
  int capacidad = (nivel_cm * 100) / H_TANQUE;

  // Clasificación en niveles ALTO, MEDIO y BAJO
  String nivel;
  if (capacidad >= 75) nivel = "ALTO";
  else if (capacidad >= 50) nivel = "MEDIO";
  else nivel = "BAJO";

  // Si el nivel del agua es menor a 25% se activa la bomba hasta que llegue al 90%
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

  // Envia al servidor un JSON con la estructura nivel_cm, clasificacion, estado de la bomba y la capacidad en %
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

    Serial.println("Enviando JSON:");
    Serial.println(json);

    int httpResponseCode = http.POST(json);
    Serial.print("HTTP Response: ");
    Serial.println(httpResponseCode);
    http.end();
  }

  // Delay de 1.5 segundos entre cada lectura
  delay(1500);
}
