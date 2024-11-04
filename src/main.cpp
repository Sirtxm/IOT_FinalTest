# include <DNSServer.h>  
# include <ESP8266WebServer.h>
# include <ESP8266WiFi.h>
# include <WiFiManager.h>
# include <PubSubClient.h>
# include <ArduinoJson.h>
# include <SPI.h>
# include <MFRC522.h>

// ตั้งค่าขาและพอร์ตต่างๆ ที่จะใช้งาน
# define RST_PIN D3     // Reset PIN สำหรับโมดูล RFID
# define SS_PIN D8      // Slave Select PIN สำหรับโมดูล RFID
# define LED_PIN D1     // PIN ที่ต่อกับ LED เพื่อแสดงสถานะการเปิดปิดประตู

// ข้อมูลการเชื่อมต่อกับ MQTT Broker
const char *mqtt_broker = "broker.emqx.io";          // Broker ที่จะเชื่อมต่อ
const char *mqtt_topic_pub = "RFID_PORBLE";    // หัวข้อที่ใช้ส่งข้อมูล
const char *mqtt_topic_sub = "RFID_PORBLE/SUB"; // หัวข้อที่ใช้รับข้อมูล
const int mqtt_port = 1883;                          // พอร์ตของ MQTT Broker

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
MFRC522 mfrc522(SS_PIN, RST_PIN);

String rfid_in = "";     // ตัวแปรสำหรับเก็บค่า UID ของบัตร RFID
String mqttMessage;      // ตัวแปรสำหรับเก็บข้อความที่ได้รับจาก MQTT
int state;

// กำหนดสถานะต่างๆ ของโปรแกรม
const int CARD_WAIT = 0;
const int CARD_TOUCH = 1;
const int RECEIVE_RFID = 2;

// JSON Document สำหรับเก็บข้อมูลส่งออกและรับเข้า
DynamicJsonDocument doc_pub(256);
DynamicJsonDocument doc_sub(256);
char jsonBuffer[256]; // Buffer สำหรับเก็บ JSON ที่จะส่งออก

void setupWiFiManager();
void connectToMQTTBroker();
void mqttCallback(char *topic, byte *payload, unsigned int length);
String dump_byte_array(byte *buffer, byte bufferSize);

void setup() {
  state = CARD_WAIT;
  Serial.begin(115200);  // เริ่มต้น Serial Monitor สำหรับการดีบัก

  // กำหนดค่า PIN และเริ่มต้นการทำงานของโมดูล RFID
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  SPI.begin();
  mfrc522.PCD_Init();    // เริ่มต้นโมดูล RFID
  mfrc522.PCD_DumpVersionToSerial();

  setupWiFiManager();    // เชื่อมต่อ WiFi

  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);  // ตั้งค่า callback function
  connectToMQTTBroker();
}

void loop() {
  // จัดการสถานะต่างๆ ภายในโปรแกรม
  if (state == CARD_WAIT) {
    // รอให้มีการแตะบัตร RFID
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      state = CARD_TOUCH;
    }
  }
  else if (state == CARD_TOUCH) {
    // อ่าน UID ของบัตรและส่งข้อมูลไปยัง Server ผ่าน MQTT
    rfid_in = dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.print("Card UID: ");
    Serial.println(rfid_in);

    doc_pub["rfid"] = rfid_in;  // สร้าง JSON ที่เก็บ UID ของบัตร
    serializeJson(doc_pub, jsonBuffer);
    mqtt_client.publish(mqtt_topic_pub, jsonBuffer); // ส่งข้อมูลผ่าน MQTT

    state = RECEIVE_RFID;  // เปลี่ยนสถานะเป็นรอการตอบกลับจาก Server
  }
  else if (state == RECEIVE_RFID) {
    mqtt_client.loop(); // ตรวจสอบการเชื่อมต่อ MQTT และรับข้อมูลจาก Server

    if (mqttMessage.length() > 0) {
      const char* doorStatus = doc_sub["door"];
      if (doorStatus && String(doorStatus) == "open") {
        Serial.println("Door open command received");

        // เปิด LED เป็นเวลา 3 วินาที
        digitalWrite(LED_PIN, HIGH);
        delay(3000);
        digitalWrite(LED_PIN, LOW);

        state = CARD_WAIT;
      } else {
        Serial.println("Access Denied or Invalid Command");
        state = CARD_WAIT;
      }
      mqttMessage = ""; // ล้างข้อความหลังจากประมวลผลเสร็จแล้ว
    }
  }
}

// ฟังก์ชันเชื่อมต่อ WiFi
void setupWiFiManager() {
  WiFiManager wm;
  wm.resetSettings();
  bool res = wm.autoConnect("PorBle1");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected to WiFi");
  }
}

// ฟังก์ชันเชื่อมต่อ MQTT Broker
void connectToMQTTBroker() {
  while (!mqtt_client.connected()) {
    String client_id = "esp8266-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s...\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str())) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_topic_sub);
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// ฟังก์ชัน callback เมื่อได้รับข้อความจาก MQTT Broker
void mqttCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);
    mqttMessage = "";  
    for (unsigned int i = 0; i < length; i++) {
        mqttMessage += (char)payload[i];  // แปลง payload เป็น string
    }
    deserializeJson(doc_sub, mqttMessage); // แปลงข้อมูลเป็น JSON และเก็บใน doc_sub
    const char* doc_sub_message = doc_sub["message"];
    Serial.println(doc_sub_message);
}

// ฟังก์ชันอ่าน UID ของบัตร RFID
String dump_byte_array(byte *buffer, byte bufferSize) {
  String content = "";
  for (byte i = 0; i < bufferSize; i++) {
    content.concat(String(buffer[i] < 0x10 ? " 0" : " "));
    content.concat(String(buffer[i], HEX));
  }
  content.toUpperCase();
  return content;
}