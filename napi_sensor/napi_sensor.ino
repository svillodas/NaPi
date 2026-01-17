#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SparkFun_MMA8452Q.h>
#include <ArduinoJson.h>

// --- CONFIGURACIÓN WIFI Y MQTT ---
const char* ssid = "DIOT37";      
const char* password = "dispositivos37";  
const char* mqtt_local_ip = "10.34.105.77";
const char* mqtt_cloudlet_host = "10.34.105.78";
const int mqtt_port = 1883;
const char* mqtt_topic = "NAPI/SVZ"; 

// --- VARIABLES DE ESTADO Y TIEMPO ---
const char* current_broker = "";
unsigned long lastRetryTime = 0; 
const unsigned long retryInterval = 30000; // INTENTAR VOLVER A RASPBERRY CADA 30 SEGUNDOS

// --- OBJETOS Y PINES ---
WiFiClient espClient;
PubSubClient client(espClient);
MMA8452Q accel;
const int pulsePin = A0; 
const int buttonPin = D2; 

// Variables sensores
double alpha = 0.75;
static double oldValue = 0;
double filteredPulseValue = 0; 
float impactThreshold = 3.0; 
float currentG = 0; 
unsigned long lastMessageTime = 0;
unsigned long lastTelemetryTime = 0; 
const int telemetryInterval = 1500;

void setup_wifi() {
  delay(10);
  Serial.print("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi conectada");
}

void smartReconnect() {
  while (!client.connected()) {
    String clientId = "MonitorSalud-" + String(random(0xffff), HEX);

    // Raspberry Pi
    Serial.println("Intentando conectar a Raspberry Pi...");
    client.setServer(mqtt_local_ip, mqtt_port);
    // Timeout corto de conexión para no bloquear mucho el dispositivo
    if (client.connect(clientId.c_str())) {
      Serial.println("¡Conectado a la Raspberry Pi!");
      current_broker = mqtt_local_ip;
      return; 
    }

    // Broker Público
    Serial.println("Raspberry no disponible. Conectando a Broker Público...");
    client.setServer(mqtt_cloudlet_host, mqtt_port);
    if (client.connect(clientId.c_str())) {
      Serial.println("¡Conectado al Broker Público!");
      current_broker = mqtt_cloudlet_host;
      lastRetryTime = millis(); // Empezamos a contar el tiempo para el próximo reintento local
      return;
    }

    Serial.println("Fallo total de conexión. Reintentando en 5 segundos...");
    delay(5000);
  }
}

void sendAlarm(String reason) {
  if (millis() - lastMessageTime > 5000) { 
    JsonDocument doc;
    doc["type"] = "ALARM";
    doc["alarm"] = true;
    doc["reason"] = reason;
    doc["broker"] = (current_broker == mqtt_local_ip) ? "Raspberry" : "Publico";

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(mqtt_topic, buffer);
    
    Serial.println(">> ALARMA ENVIADA vía " + String(current_broker));
    lastMessageTime = millis();
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
  pinMode(buttonPin, INPUT); 
  if (accel.init() == false) {
    Serial.println("Error: Acelerómetro");
    while (1);
  }
  setup_wifi();
  smartReconnect();
}

void loop() {
  // Verificar si estamos conectados
  if (!client.connected()) {
    smartReconnect();
  }

  // LÓGICA DE RETORNO A RASPBERRY
  if (current_broker == mqtt_cloudlet_host) {
    if (millis() - lastRetryTime > retryInterval) {
      Serial.println("--- Tiempo de prueba cumplido. Intentando volver a Raspberry... ---");
      client.disconnect(); // Forzamos desconexión para que el siguiente loop llame a smartReconnect
      lastRetryTime = millis(); // Reiniciamos el contador
      return; // Saltamos el resto del loop para ir directo a la reconexión
    }
  }

  client.loop();

  // --- Procesamiento de Sensores ---
  int rawValue = analogRead(pulsePin);
  filteredPulseValue = alpha * oldValue + (1 - alpha) * rawValue;
  oldValue = rawValue;
  
  if (accel.available()) {
    accel.read();
    currentG = sqrt(pow(accel.cx, 2) + pow(accel.cy, 2) + pow(accel.cz, 2));
  }

  // --- Acciones ---
  if (digitalRead(buttonPin) == LOW) {
      sendAlarm("ALERTA_MANUAL"); 
  }

  if (filteredPulseValue < 1000) { 
      sendAlarm("PULSO_PERDIDO");
  }
  if (currentG > impactThreshold) {
      sendAlarm("CAIDA_DETECTADA");
  }

  if (millis() - lastTelemetryTime > telemetryInterval) {
      sendTelemetry();
      lastTelemetryTime = millis();
  }

  delay(10);
}