#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // libreria de Benoit Blanchon que hemos descargado para parsear los json

// --- CREDENCIALES WIFI ---
const char* ssid = "DIOT37";      
const char* password = "dispositivos37";  

// --- CONFIGURACIÓN MQTT ---
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "NAPI/SVZ"; 

// --- DEFINICIÓN DE PINES---
const int pirPin = D3; 
const int buzzerPin = D2; 

// --- VARIABLES GLOBALES ---
bool alarmArmed = false;

WiFiClient espClient;
PubSubClient client(espClient);

// Función para conectar a WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi conectada!");
  Serial.println("IP: ");
  Serial.println(WiFi.localIP());
}

// Función que recibe los mensajes MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido en topic: ");
  Serial.println(topic);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("Error al parsear JSON: ");
    Serial.println(error.c_str());
    return; 
  }
  if (doc.containsKey("alarm")) {
    bool alarmStatus = doc["alarm"];

    if (alarmStatus == true) {
      alarmArmed = true;
      Serial.println("-> COMANDO RECIBIDO: ALARMA ARMADA");
    } else {
      alarmArmed = false;
      digitalWrite(buzzerPin, LOW); // Apagamos buzzer por si acaso estaba sonando
      Serial.println("-> COMANDO RECIBIDO: ALARMA DESARMADA");
    }
  } 
  else {
    Serial.println("El JSON recibido no tiene la clave 'alarm'");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a broker MQTT...");
    String clientId = "FireBeetle-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("conectado!");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println(" intentando en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(pirPin, INPUT);
  digitalWrite(buzzerPin, LOW);
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected())reconnect();
  client.loop();
  // Leemos el sensor PIR (HIGH = movimiento detectado)
  int motionDetected = digitalRead(pirPin);
  // Solo suena SI la alarma fue armada por MQTT Y hay movimiento
  if (alarmArmed == true && motionDetected == HIGH) {
    Serial.println("¡AYUDA CERCA! ACTIVANDO AVISOS!!! SOS!!!");
    digitalWrite(buzzerPin, HIGH); // Suena el buzzer
  } else digitalWrite(buzzerPin, LOW);
  delay(100);
}