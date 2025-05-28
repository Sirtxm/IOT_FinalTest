#include <DNSServer.h>  
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// Define pins and ports used for hardware setup
#define RST_PIN D3       // Reset pin for the RFID module
#define SS_PIN D8        // Slave Select pin for the RFID module
#define LED_PIN D1       // LED pin to indicate door status

// MQTT Broker configuration
const char *mqtt_broker = "broker.emqx.io";         // MQTT broker address
const char *mqtt_topic_pub = "RFID_PORBLE";         // Topic to publish RFID UID
const char *mqtt_topic_sub = "RFID_PORBLE/SUB";     // Topic to subscribe for door control
const int mqtt_port = 1883;                         // Port for MQTT broker

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
MFRC522 mfrc522(SS_PIN, RST_PIN);                   // Initialize RFID reader

String rfid_in = "";        // Stores UID read from RFID card
String mqttMessage;         // Stores message received from MQTT broker
int state;

// Program states
const int CARD_WAIT = 0;       // Waiting for card
const int CARD_TOUCH = 1;      // Card detected
const int RECEIVE_RFID = 2;    // Waiting for response from broker

// JSON documents for sending and receiving MQTT data
DynamicJsonDocument doc_pub(256);
DynamicJsonDocument doc_sub(256);
char jsonBuffer[256];         // Buffer for serialized JSON

// Function declarations
void setupWiFiManager();
void connectToMQTTBroker();
void mqttCallback(char *topic, byte *payload, unsigned int length);
String dump_byte_array(byte *buffer, byte bufferSize);

void setup() {
  state = CARD_WAIT;
  Serial.begin(115200);  // Start serial communication

  // Initialize RFID and set LED off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  SPI.begin();               // Start SPI bus
  mfrc522.PCD_Init();        // Initialize RFID module
  mfrc522.PCD_DumpVersionToSerial();

  setupWiFiManager();        // Connect to WiFi

  mqtt_client.setServer(mqtt_broker, mqtt_port);     // Set MQTT server
  mqtt_client.setCallback(mqttCallback);             // Register MQTT callback
  connectToMQTTBroker();                             // Connect to MQTT broker
}

void loop() {
  // Handle card detection and MQTT communication
  if (state == CARD_WAIT) {
    // Wait for new RFID card
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      state = CARD_TOUCH;
    }
  }
  else if (state == CARD_TOUCH) {
    // Read card UID and send it via MQTT
    rfid_in = dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.print("Card UID: ");
    Serial.println(rfid_in);

    doc_pub["rfid"] = rfid_in;                       // Create JSON payload
    serializeJson(doc_pub, jsonBuffer);              // Serialize JSON
    mqtt_client.publish(mqtt_topic_pub, jsonBuffer); // Publish to MQTT

    state = RECEIVE_RFID;  // Wait for door status response
  }
  else if (state == RECEIVE_RFID) {
    mqtt_client.loop();  // Handle incoming MQTT messages

    if (mqttMessage.length() > 0) {
      const char* doorStatus = doc_sub["door"];
      if (doorStatus && String(doorStatus) == "open") {
        Serial.println("Door open command received");

        // Simulate door unlock with LED on for 3 seconds
        digitalWrite(LED_PIN, HIGH);
        delay(3000);
        digitalWrite(LED_PIN, LOW);

        state = CARD_WAIT;
      } else {
        Serial.println("Access Denied or Invalid Command");
        state = CARD_WAIT;
      }
      mqttMessage = "";  // Clear message after processing
    }
  }
}

// Connect to WiFi using WiFiManager portal
void setupWiFiManager() {
  WiFiManager wm;
  wm.resetSettings(); // Reset saved credentials (optional)
  bool res = wm.autoConnect("PorBle1"); // Create access point if no WiFi
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected to WiFi");
  }
}

// Connect to the MQTT broker with retry logic
void connectToMQTTBroker() {
  while (!mqtt_client.connected()) {
    String client_id = "esp8266-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s...\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str())) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_topic_sub);  // Subscribe to topic
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Callback function when a message is received from the broker
void mqttCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);

    mqttMessage = "";  
    for (unsigned int i = 0; i < length; i++) {
        mqttMessage += (char)payload[i];  // Convert payload to string
    }

    deserializeJson(doc_sub, mqttMessage); // Parse JSON from MQTT message
    const char* doc_sub_message = doc_sub["message"];
    Serial.println(doc_sub_message);       // Optional debug output
}

// Helper function to convert UID bytes to string
String dump_byte_array(byte *buffer, byte bufferSize) {
  String content = "";
  for (byte i = 0; i < bufferSize; i++) {
    content.concat(String(buffer[i] < 0x10 ? " 0" : " "));
    content.concat(String(buffer[i], HEX));
  }
  content.toUpperCase();
  return content;
}
