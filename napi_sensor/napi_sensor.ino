#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SparkFun_MMA8452Q.h>
#include <ArduinoJson.h>

// --- CONFIGURACIÓN WIFI Y MQTT ---
const char* ssid = "DIOT37";      
const char* password = "dispositivos37";  
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "NAPI/SVZ"; 

// --- CONFIGURACIÓN PINES ---
const int pulsePin = A0; 
const int buttonPin = D2; 

// --- OBJETOS ---
WiFiClient espClient;
PubSubClient client(espClient);
MMA8452Q accel; // Acelerómetro

// --- VARIABLES PULSO ---
double alpha = 0.75;
static double oldValue = 0;
double filteredPulseValue = 0; 

// --- VARIABLES CAÍDA ---
float impactThreshold = 3.0; 
float currentG = 0; 

// --- CONTROL DE TIEMPO ---
unsigned long lastMessageTime = 0;
bool alarmActive = false; 

unsigned long lastTelemetryTime = 0; 
const int telemetryInterval = 200; // Enviar datos cada 200ms

void setup_wifi() {
  delay(10);
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi conectada");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    String clientId = "MonitorSalud-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!");
    } else {
      delay(5000);
    }
  }
}

// Función genérica para enviar alarmas
void sendAlarm(String reason) {
  // Verificamos que hayan pasado al menos 5 segundos desde la última alarma
  // para evitar "spam" de mensajes si el usuario mantiene el botón presionado.
  if (millis() - lastMessageTime > 5000) { 
    JsonDocument doc;
    doc["type"] = "ALARM"; // Etiqueta para diferenciar
    doc["alarm"] = true;
    doc["reason"] = reason;

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(mqtt_topic, buffer);
    
    Serial.println(">> ALARMA ENVIADA: " + reason); // Debug en consola
    lastMessageTime = millis();
    alarmActive = true;
  }
}

void sendTelemetry() {
    JsonDocument doc;
    doc["type"] = "DATA"; 
    doc["pulse_val"] = (int)filteredPulseValue; 
    doc["g_force"] = currentG;                 

    char buffer[256];
    serializeJson(doc, buffer);
    
    client.publish(mqtt_topic, buffer);
}

void setup() {
  Serial.begin(9600); 
  Wire.begin(); 
  
  // Configuración del botón
  pinMode(buttonPin, INPUT); 
  if (accel.init() == false) {
    Serial.println("Error: Acelerómetro");
    while (1);
  }

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // --- LECTURA DE SENSORES ---
  int rawValue = analogRead(pulsePin);
  
  // Fórmula del filtro paso bajo
  filteredPulseValue = alpha * oldValue + (1 - alpha) * rawValue;
  oldValue = rawValue;
  
  if (accel.available()) {
    accel.read();
    // Pitágoras: magnitud del vector
    currentG = sqrt(pow(accel.cx, 2) + pow(accel.cy, 2) + pow(accel.cz, 2));
  }

  // --- LECTURA DEL BOTÓN DE PÁNICO ---
  if (digitalRead(buttonPin) == LOW) {
      sendAlarm("ALERTA_MANUAL"); 
  }

  // --- LÓGICA DE ALARMAS AUTOMÁTICAS ---
  if (filteredPulseValue < 1000) { 
      sendAlarm("PULSO_PERDIDO");
  }
  if (currentG > impactThreshold) {
      sendAlarm("CAIDA_DETECTADA");
  }

  // --- ENVÍO DE TELEMETRÍA ---
  if (millis() - lastTelemetryTime > telemetryInterval) {
      sendTelemetry();
      lastTelemetryTime = millis();
  }

  // VISUALIZACIÓN LOCAL (Serial Plotter)
  // Serial.print(rawValue);          
  // Serial.print(",");               
  // Serial.print(filteredPulseValue);
  // Serial.print(",");
  // Serial.println(currentG * 1000); 
  
  delay(100); 
}