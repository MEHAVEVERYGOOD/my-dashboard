#include <WiFiS3.h>
#include <PubSubClient.h>

// --- ตั้งค่า WiFi และ MQTT ---
const char* ssid = "1T1M";
const char* password = "12345678";
const char* mqtt_server = "broker.hivemq.com";

// --- พินอุปกรณ์ (ใช้ตัวแปรเดิม) ---
const int XKC_PIN = 2;   // เซนเซอร์ดักไขมัน
const int PUMP_PIN = 4;  // พินปั๊ม (LED จำลอง)

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
bool greaseDetected = false;

// --- ฟังก์ชันรับคำสั่งจาก Dashboard ---
void callback(char* topic, byte* payload, unsigned int length) {
  char message = (char)payload[0];
  
  if (String(topic) == "myiot/control") {
    if (message == '1') {
      // สั่งเปิด: ต้องเช็คความปลอดภัย (Safety Check)
      if (greaseDetected) {
        digitalWrite(PUMP_PIN, HIGH);
        client.publish("myiot/pump_real_status", "1");
      }
    } 
    else if (message == '0') {
      // สั่งปิด: ต้องปิดทันที ไม่ต้องเช็คเงื่อนไขอื่น (Emergency Stop)
      digitalWrite(PUMP_PIN, LOW);
      client.publish("myiot/pump_real_status", "0");
      Serial.println("Manual STOP received");
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(XKC_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  
  // เชื่อมต่อ WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nWiFi Connected");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "GreaseProject-Final-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe("myiot/control");
      Serial.println("MQTT Connected");
    } else {
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 500) { // ทำงานทุก 3 วินาที
    lastMsg = now;

    // 1. อ่านค่าเซนเซอร์ดักไขมัน
    int sensorState = digitalRead(XKC_PIN);
    greaseDetected = (sensorState == HIGH); 
    client.publish("myiot/grease_status", String(sensorState).c_str());

    // 2. สุ่มตัวเลขแทนอุณหภูมิ (ส่งไปให้ HTML แสดงผล)
    int fakeTemp = random(25, 36);
    client.publish("myiot/temp", String(fakeTemp).c_str());

    // 3. ระบบ Safety: ถ้าไขมันหายไป (ดูดจนหมด) ให้สั่งปิดปั๊มทันที
    if (!greaseDetected && digitalRead(PUMP_PIN) == HIGH) {
      digitalWrite(PUMP_PIN, LOW);
      client.publish("myiot/pump_real_status", "0");
    }

    // แสดงค่าผ่าน Serial Monitor สำหรับตรวจสอบ
    Serial.print("Grease: "); Serial.print(sensorState ? "DETECTED" : "EMPTY");
    Serial.print(" | Sim Temp: "); Serial.println(fakeTemp);
  }
  if (now - lastMsg > 500) {
    lastMsg = now;
    
    // ... โค้ดอ่านค่า XKC และสุ่ม Temp เดิม ...

    // ส่งสถานะปัจจุบันของปั๊มเพื่อไปวาดกราฟแบบ Real-time
    int currentPumpState = digitalRead(PUMP_PIN);
    client.publish("myiot/pump_real_status", String(currentPumpState).c_str());
    
    // ...
}
}