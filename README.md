# 📡 RFID_MQTT_ESP8266

A smart IoT-based door access system using **RFID**, **ESP8266**, and **MQTT**. This project scans RFID cards, sends the UID to a cloud MQTT broker, and unlocks the door (simulated by LED) upon receiving a valid response.

---

## 🚀 Features

- 🔐 RFID-based user authentication
- ☁️ MQTT communication (publish/subscribe)
- 📶 WiFi provisioning with **WiFiManager**
- 📄 JSON data encoding with **ArduinoJson**
- 💡 LED status for door control simulation
- 🧠 State-driven logic: `WAIT → TOUCH → RECEIVE`

---

## 🧰 Hardware Requirements

- ESP8266 (e.g. NodeMCU)
- MFRC522 RFID module
- LED and resistor
- RFID tag or card
- Breadboard and jumper wires
- WiFi internet access

---

## 📡 MQTT Configuration

| Setting         | Value               |
|------------------|---------------------|
| Broker           | `broker.emqx.io`    |
| Port             | `1883`              |
| Publish Topic    | `RFID_PORBLE`       |
| Subscribe Topic  | `RFID_PORBLE/SUB`   |

### Example Publish Message

```json
{
  "rfid": "A1 B2 C3 D4"
}
