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

// URL para envío y control
String serverUpdateUrl = "https://SERVER_RENDER.com/api/update";
String serverConfigUrl = "https://SERVER_RENDER.com/api/control";

// Config del LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Memoria no volátil (EEPROM)
Preferences prefs;

// Variables globales del sistema
float OFFSET_SENSOR = 2.0;     // Protección del sensor (2 cm)
float H_TANQUE = 0.0;          // Altura total del tanque
bool calibrado = false;        // Indica si el tanque fue calibrado
bool bomba = false;            // Estado de la bomba

// Variables de medición y control
float distancia_cm = 0;
float nivel_cm = 0;
int capacidad = 0;
String nivel = "BAJO";

// Variables de temporización (millis)
unsigned long tSensor = 0;
unsigned long tLCD = 0;
unsigned long tSend = 0;
unsigned long tControl = 0;

// Intervalows de ejecución (no bloqueantes)
const unsigned long SENSOR_INTERVAL  = 200;   // Lectura del sensor
const unsigned long LCD_INTERVAL     = 200;   // Actualización del LCD
const unsigned long SEND_INTERVAL    = 3000;  // Envío HTTP
const unsigned long CONTROL_INTERVAL = 5000;  // Control remoto

// Funcion para leer distancia del sensor ultrasónico
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

// Funcion de actualización del sensor y control de bomba
void actualizarSensor() {
  float d = readDistance();
  if (d < 0) return;

  distancia_cm = d;

  float d_efectiva = d - OFFSET_SENSOR;
  if (d_efectiva < 0) d_efectiva = 0;

  nivel_cm = H_TANQUE - d_efectiva;
  nivel_cm = constrain(nivel_cm, 0, H_TANQUE);

  capacidad = (nivel_cm * 100) / H_TANQUE;

  if (capacidad >= 75) nivel = "ALTO";
  else if (capacidad >= 50) nivel = "MEDIO";
  else nivel = "BAJO";

  if (!bomba && capacidad <= 25) bomba = true;
  if (bomba && capacidad >= 90) bomba = false;

  digitalWrite(RELE_PIN, bomba ? HIGH : LOW);
}

// Funcion para actualizar el LCD
void actualizarLCD() {
  if (!calibrado) {
    lcd.setCursor(0, 0);
    lcd.print("No calibrado   ");
    lcd.setCursor(0, 1);
    lcd.print("Esperando...   ");
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("Nivel: ");
  lcd.print(nivel_cm, 2);
  lcd.print("cm   ");

  lcd.setCursor(0, 1);
  lcd.print(capacidad);
  lcd.print("% B:");
  lcd.print(bomba ? "ON     " : "OFF   ");
}

// Funcion para enviar datos al servidor (HTTP POST)
void enviarServidor() {
  if (WiFi.status() != WL_CONNECTED || !calibrado) return;

  HTTPClient http;
  http.setTimeout(800);
  http.begin(serverUpdateUrl);
  http.addHeader("Content-Type", "application/json");

  String json = "{"
    "\"nivel_cm\":" + String(nivel_cm, 2) +
    ",\"nivel\":\"" + nivel +
    "\",\"bomba\":" + String(bomba) +
    ",\"capacidad\":" + String(capacidad) +
    ",\"h_tanque\":" + String(H_TANQUE, 2) +
  "}";

  http.POST(json);
  http.end();
}

// Funcion de calibracion del tanque
float calibrarTanque() {
  float suma = 0;
  int muestras = 10;

  for (int i = 0; i < muestras; i++) {
    float d = readDistance();
    if (d > 0) suma += d;
    delay(150);   // SOLO durante calibración
  }
  return suma / muestras;
}

// Funcion para guardar la altura en EEPROM
void guardarAltura(float altura) {
  prefs.begin("tanque", false);
  prefs.putFloat("altura", altura);
  prefs.end();

  H_TANQUE = altura;
  calibrado = true;
}

// Funcion para verificar solicitudes del servidor
void verificarServidor() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(800);
  http.begin(serverConfigUrl);

  if (http.GET() == 200) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, http.getString());

    if (doc["calibrar"]) {
      bomba = false;
      lcd.clear();
      lcd.print("Calibrando...");

      float altura = calibrarTanque() - OFFSET_SENSOR;
      guardarAltura(altura);

      bomba = false;
      lcd.clear();
      lcd.print("Altura: ");
      lcd.print(altura, 2);
      lcd.print("cm");
      delay(2000);
    }
  }
  http.end();
}

// SETUP
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

  lcd.print("Conectando...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  lcd.clear();
  lcd.print("WiFi conectado");
  delay(1000);
  lcd.clear();
}

// LOOP
void loop() {
  unsigned long now = millis();

  if (now - tSensor >= SENSOR_INTERVAL) {
    tSensor = now;
    if (calibrado) actualizarSensor();
  }

  if (now - tLCD >= LCD_INTERVAL) {
    tLCD = now;
    actualizarLCD();
  }

  if (now - tSend >= SEND_INTERVAL) {
    tSend = now;
    enviarServidor();
  }

  if (now - tControl >= CONTROL_INTERVAL) {
    tControl = now;
    verificarServidor();
  }
}
