#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- CREDENCIALES WIFI ---
const char* ssid = "DIOT37";      
const char* password = "dispositivos37";  

// --- CONFIGURACIÓN MQTT DUAL ---
const char* mqtt_local_ip = "10.34.105.77";
const char* mqtt_public_host = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "NAPI/SVZ"; 

// --- VARIABLES DE ESTADO Y TIEMPO ---
const char* current_broker = "";
unsigned long lastRetryTime = 0; 
const unsigned long retryInterval = 30000; 

// --- VARIABLES PARA AUTO-APAGADO ---
unsigned long alarmActivationTime = 0;
const unsigned long alarmDuration = 10000;
bool alarmActivate = false;

// --- PINES ---
const int pirPin = D3; 
const int buzzerPin = D2; 

bool alarmArmed = false;
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.print("\nConectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi conectada!");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Mensaje recibido en [%s]\n", topic);
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) return;

  if (doc.containsKey("alarm")) {
    alarmArmed = doc["alarm"];
    if (alarmArmed) {
      Serial.println("-> SISTEMA ARMADO");
    } else {
      digitalWrite(buzzerPin, LOW);
      alarmArmed = false;
      alarmActivate = false;
      Serial.println("-> SISTEMA DESARMADO");
    }
  }
}

void smartReconnect() {
  while (!client.connected()) {
    String clientId = "Actuador-" + String(random(0xffff), HEX);
    client.setServer(mqtt_local_ip, mqtt_port);
    if (client.connect(clientId.c_str())) {
      client.subscribe(mqtt_topic);
      current_broker = mqtt_local_ip;
      Serial.println("Conectado a Raspberry");
      return; 
    }
    client.setServer(mqtt_public_host, mqtt_port);
    if (client.connect(clientId.c_str())) {
      client.subscribe(mqtt_topic);
      current_broker = mqtt_public_host;
      lastRetryTime = millis();
      Serial.println("Conectado a Broker Público");
      return;
    }
    delay(5000);
  }
}

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(pirPin, INPUT);
  digitalWrite(buzzerPin, LOW);
  Serial.begin(9600);
  
  setup_wifi();
  client.setCallback(callback);
  smartReconnect();
}

void loop() {
  if (!client.connected()) {
    smartReconnect();
  }

  if (current_broker == mqtt_public_host) {
    if (millis() - lastRetryTime > retryInterval) {
      client.disconnect(); 
      lastRetryTime = millis();
      return;
    }
  }

  client.loop();

  int motionDetected = digitalRead(pirPin);
  
  // Activar si está armado, hay movimiento y NO estaba ya sonando
  if (alarmArmed && motionDetected == HIGH && !alarmActivate) {
    Serial.println("¡Movimiento detectado! Alarma activa por 10s.");
    digitalWrite(buzzerPin, HIGH);
    alarmActivationTime = millis();
    alarmActivate = true;
  }

  // Apagar si el buzzer está activo y ya pasaron los 10 segundos
  if (alarmActivate && (millis() - alarmActivationTime >= alarmDuration)) {
    Serial.println("Tiempo cumplido. Apagando alarma.");
    digitalWrite(buzzerPin, LOW);
    alarmArmed = false;
    alarmActivate = false;
  }
  
  delay(10); 
}