#include <WiFiS3.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- 1. ตั้งค่า WiFi และ MQTT ---
const char* ssid = "1T1M";
const char* password = "12345678";
const char* mqtt_server = "broker.hivemq.com";

// --- 2. กำหนดพินอุปกรณ์ ---
const int XKC_1_PIN = 2;     
const int XKC_2_PIN = 3;     
const int PUMP_PIN = 4;      
const int TEMP_PIN = 5;      

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTempMsg = 0;
int lastSentState = -1;
int lastSentPump = -1;
bool isGrease = false; 

// ฟังก์ชันส่งสถานะปั๊ม (ปรับให้ส่งค่า 1 เมื่อ Pin เป็น LOW สำหรับ Relay Active Low)
void sendPumpStatus() {
  int pinVal = digitalRead(PUMP_PIN);
  int logicalStatus = (pinVal == LOW) ? 1 : 0; // LOW คือเปิด(1), HIGH คือปิด(0)
  client.publish("myiot/pump_real_status", String(logicalStatus).c_str());
  lastSentPump = logicalStatus;
}

void callback(char* topic, byte* payload, unsigned int length) {
  char message = (char)payload[0];
  if (String(topic) == "myiot/control") {
    if (message == '1' && isGrease) {
      digitalWrite(PUMP_PIN, LOW); // ส่ง LOW เพื่อให้ Relay "ติด"
      sendPumpStatus();
    } else if (message == '0') {
      digitalWrite(PUMP_PIN, HIGH); // ส่ง HIGH เพื่อให้ Relay "ดับ"
      sendPumpStatus();
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(XKC_1_PIN, INPUT);
  pinMode(XKC_2_PIN, INPUT);
  
  // สำคัญ: ต้องสั่งให้ Relay เป็น HIGH ทันทีที่เริ่ม เพื่อให้ปั๊ม "ปิด" อยู่เสมอตอนเปิดเครื่อง
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH); 
  
  sensors.begin();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) { reconnect(); }
  client.loop();

  int s1 = digitalRead(XKC_1_PIN);
  int s2 = digitalRead(XKC_2_PIN);
  int currentState = 0;

  // Logic แยกวัสดุ (เหมือนเดิม)
  if (s1 == LOW && s2 == LOW) { currentState = 0; isGrease = false; } 
  else if (s1 != s2) { currentState = 1; isGrease = true; } 
  else if (s1 == HIGH && s2 == HIGH) { currentState = 2; isGrease = false; }

  if (currentState != lastSentState) {
    client.publish("myiot/grease_status", String(currentState).c_str());
    lastSentState = currentState;
    
    // Safety: ถ้าสถานะเปลี่ยนเป็นไม่ใช่ไขมัน ให้ "ดับ" ปั๊มทันที (ส่ง HIGH)
    if (currentState != 1 && digitalRead(PUMP_PIN) == LOW) {
      digitalWrite(PUMP_PIN, HIGH);
      sendPumpStatus();
    }
  }

  unsigned long now = millis();
  if (now - lastTempMsg > 5000) {
    lastTempMsg = now;
    sensors.requestTemperatures(); 
    float tempC = sensors.getTempCByIndex(0);
    if(tempC != DEVICE_DISCONNECTED_C) {
      client.publish("myiot/temp", String(tempC, 1).c_str());
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("GreaseR4_ActiveLow")) { client.subscribe("myiot/control"); } 
    else { delay(5000); }
  }
}