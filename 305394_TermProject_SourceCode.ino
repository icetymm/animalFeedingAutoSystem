#include <dummy.h>

#include <Wire.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// พินสำหรับเซ็นเซอร์และมอเตอร์
#define buzzerPin D4     // Buzzer ต่อกับ D4
#define motorPin D2      // Servo motor ต่อกับ D2
#define irSensorPin D6   // IR sensor ต่อกับ D6
#define switchPin D5     // ขาสำหรับสวิตช์ (เชื่อมต่อกับ D5)
#define potPin A0        // กำหนดขาที่เชื่อมต่อกับ Potentiometer
#define ledPin D7        // กำหนดพินสำหรับ LED (เชื่อมต่อกับ D7)
#define SDA_PIN D1       // กำหนด SDA
#define SCL_PIN D3       // กำหนด SCL

const char* ssid = "Karaket";
const char* password = "ket0614790515";
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char* mqtt_client = "cf4b04f8-1054-4276-ae37-c6aabd834acd";
const char* mqtt_username = "TgdEgYbfTiy5eqzsQuXPQZY8cRoLThYb";
const char* mqtt_password = "588dnawbLi4QWuxcPGHYSAmpjaF5oTjm";

int ledPin_Status = 0;
int switchPin_Control = 1;

char msg[100];
WiFiClient espClient;
PubSubClient client(espClient);

void reconnect() 
{
 while (!client.connected()) 
 {
 Serial.print("Attempting MQTT connection…");
 if (client.connect(mqtt_client, mqtt_username, mqtt_password)) 
 {
 Serial.println("Connected");
 
client.subscribe("@msg/ledPin_status");
client.subscribe("@msg/switchPin_control");
 }
 else 
 {
 Serial.print("failed, rc=");
 Serial.print(client.state());
 Serial.println("Try again in 5 seconds...");
 delay(5000);
 }
 }
}


Servo myServo;
int potValue = 0;           // ค่าจาก Potentiometer
int lastSwitchState = HIGH;  // เก็บสถานะล่าสุดของสวิตช์
int countdownTime = 0;       // เวลานับถอยหลัง (วินาที)
bool countingDown = false;   // สถานะการนับถอยหลัง
bool switchPressed = false;  // เก็บสถานะว่ามีการกดปุ่มแล้วหรือไม่

LiquidCrystal_I2C lcd(0x27, 16, 2); // กำหนดที่อยู่ I2C ของ LCD

// ฟังก์ชันการตรวจสอบว่าอาหารหมดหรือไม่
bool isFoodEmpty() {
  int irValue = digitalRead(irSensorPin);
  return (irValue == HIGH);  // ถ้าค่า IR เป็น HIGH แสดงว่าอาหารหมด
}

// ฟังก์ชันการให้อาหาร
void feedAnimal() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Feeding...");
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin, HIGH); // เปิด LED เมื่อเริ่มจ่ายอาหาร
  client.publish("@shadow/data/update", "{\"data\" : {\"ledPin_status\" : \"on\"}}");

  myServo.write(180);  // หมุนเซอร์โวไปที่ 180 องศาเมื่อเริ่มให้อาหาร
  Serial.println("Feeding in progress...");

  // รอจนกว่าอาหารจะเต็ม (IR sensor เปลี่ยนเป็น LOW)
  while (digitalRead(irSensorPin) == HIGH) {
    lcd.setCursor(0, 1);
    lcd.print("Food empty!");
    delay(500);  // รอ 500ms เพื่อลดภาระ CPU
  }

  // เมื่ออาหารเต็ม
  Serial.println("Food is full, stopping feeding.");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Feeding done.");
  lcd.setCursor(0, 1);
  lcd.print("Food full");
  digitalWrite(buzzerPin, HIGH);
  digitalWrite(ledPin, LOW);  // ปิด LED เมื่อจ่ายอาหารเสร็จ
  client.publish("@shadow/data/update", "{\"data\" : {\"ledPin_status\" : \"off\"}}");
  stopFeeding();  // เรียกฟังก์ชันหยุดการให้อาหาร
}

// ฟังก์ชันหยุดการให้อาหาร
void stopFeeding() {
  myServo.write(0);              // หมุนเซอร์โวกลับไปที่ 0 องศาเมื่อหยุดให้อาหาร
  digitalWrite(buzzerPin, HIGH);  // ปิดเสียง Buzzer
  Serial.println("Feeding stopped.");
}

// ฟังก์ชันการรอคอยจนกว่าจะสามารถจ่ายอาหารได้
void waitUntilCanFeed() {
  while (!isFoodEmpty()) {  // วนลูปตราบใดที่อาหารยังเต็มอยู่
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Food full, wait");
    Serial.println("Food is already full. Waiting...");
    countdownTime += 5;     // เพิ่มเวลานับถอยหลังอีก 5 วินาที
    lcd.setCursor(0, 1);
    lcd.print("Next try in 5s");
    delay(5000);            // รอ 5 วินาทีก่อนตรวจสอบอีกครั้ง
  }
  // เมื่ออาหารไม่เต็มแล้ว สามารถเริ่มให้อาหารได้
  Serial.println("Food is now empty. Starting feeding.");
  feedAnimal();             // เริ่มให้อาหาร
}

int switchState = digitalRead(switchPin);
bool remotePressed = false; // ตัวแปรเพื่อบันทึกว่ามีการกดจาก NETPIE

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

 if (String(topic) == "@msg/switchPin_control") {
    if (msg == "on") {
      Serial.println("Turning switch ON");
      remotePressed = true;  // บันทึกว่ามีการกดจาก NETPIE
      countingDown = true;   // เปิดสวิตช์
    } 
    else if (msg == "off") {
      Serial.println("Turning switch OFF");
      remotePressed = false;
      

    }
  }
}

void notifyFoodEmpty() {
    String message = "{\"data\": {\"status\": \"อาหารหมด\"}}"; // ข้อความที่ต้องการส่ง
    client.publish("@shadow/data/update", message.c_str()); // ส่งข้อความไปยัง shadow
    Serial.println("Sent message: " + message); // พิมพ์ข้อความที่ส่งใน Serial Monitor
}
void notifyFoodfull() {
    String message = "{\"data\": {\"status\": \"อาหารเต็มแล้ว\"}}"; // ข้อความที่ต้องการส่ง
    client.publish("@shadow/data/update", message.c_str()); // ส่งข้อความไปยัง shadow
    Serial.println("Sent message: " + message); // พิมพ์ข้อความที่ส่งใน Serial Monitor
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN); // เริ่มต้น I2C ที่กำหนดพอร์ต SDA และ SCL
  lcd.begin(16, 2); // เริ่มต้น LCD โดยระบุจำนวนคอลัมน์และแถว
  lcd.backlight(); // เปิดไฟแบคไลท์
  lcd.setCursor(0, 0); // ตั้งค่าตำแหน่งของเคอร์เซอร์ที่ (0, 0)
  lcd.print("System ready");
  delay(2000);
  lcd.clear();

  Serial.begin(115200);
  myServo.attach(motorPin);  // เชื่อมต่อ Servo motor เข้ากับ D2
  myServo.write(0);          // กำหนดให้เซอร์โวหมุนไปที่มุม 0 องศาเริ่มต้น
  Serial.println("Servo motor initialized.");

  pinMode(switchPin, INPUT);  // ตั้งค่าเป็น Input สำหรับสวิตช์
  pinMode(buzzerPin, OUTPUT); // ตั้งค่าเป็น Output สำหรับ Buzzer
  pinMode(irSensorPin, INPUT); // ตั้งค่าเป็น Input สำหรับ IR Sensor
  pinMode(ledPin, OUTPUT);    // ตั้งค่าเป็น Output สำหรับ LED
  digitalWrite(buzzerPin, HIGH);  // ปิดเสียงแจ้งเตือนเริ่มต้น
  digitalWrite(ledPin, LOW);      // ปิด LED เริ่มต้น
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
 {
 delay(500);
 Serial.print(".");
 }
 Serial.println("WiFi connected");
 Serial.print("IP address: ");
 Serial.println(WiFi.localIP());
 client.setServer(mqtt_server, mqtt_port);
 client.setCallback(callback);
 client.subscribe("@msg/switchPin_control");
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // ตรวจสอบและประมวลผล MQTT ทุกครั้งใน loop

  // ตรวจสอบสถานะอาหารตลอดเวลา
  lcd.setCursor(0, 0);
  if (isFoodEmpty()) {
    lcd.print("Food empty    "); // เคลียร์ข้อความเมื่ออาหารหมด
    notifyFoodEmpty();
  } else {
    lcd.print("Food full     "); // เคลียร์ข้อความเมื่ออาหารเต็ม
    notifyFoodfull();
  }

  // อ่านค่า potentiometer เพื่อตั้งเวลา
  potValue = analogRead(potPin);
  countdownTime = map(potValue, 0, 1023, 10, 600); // แปลงค่าเป็นเวลานับถอยหลัง (10 ถึง 600 วินาที)
  
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(countdownTime);
  lcd.print(" sec");

  Serial.print("Countdown time set to: ");
  Serial.print(countdownTime);
  Serial.println(" seconds.");

  int switchState = digitalRead(switchPin);

  // พิมพ์สถานะของสวิตช์ออกมาใน Serial Monitor
  Serial.print("Switch state: ");
  Serial.println(switchState);

  // ตรวจสอบการเปลี่ยนแปลงสถานะของสวิตช์เมื่อยังไม่เริ่มนับถอยหลัง
  if (switchState == LOW && lastSwitchState == HIGH && !countingDown && !switchPressed) {
    Serial.println("Switch pressed! Starting countdown...");
    countingDown = true; // เริ่มนับถอยหลังเมื่อกดสวิตช์
    switchPressed = true; // บันทึกว่าปุ่มถูกกดแล้ว
  }

  // รีเซ็ตสถานะ switchPressed เมื่อปล่อยปุ่ม
  if (switchState == HIGH && lastSwitchState == LOW) {
    switchPressed = false;
  }

  lastSwitchState = switchState;

  // ถ้ากำลังนับถอยหลัง
  if (countingDown) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Counting down...");

    Serial.println("Counting down...");
    while (countdownTime > 0) {
      lcd.setCursor(0, 1);
      lcd.print("Time left: ");
      lcd.print(countdownTime);
      delay(1000); // รอ 1 วินาที
      countdownTime--;
      client.loop();
    }

    // เมื่อเวลานับถอยหลังถึงศูนย์
    Serial.println("Time's up! Checking food status...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time's up!");

    // ถ้าอาหารเต็ม จะเรียกฟังก์ชันใหม่
    if (!isFoodEmpty()) {
      waitUntilCanFeed();  // เริ่มฟังก์ชันรอคอยจนสามารถจ่ายอาหารได้
    } else {
      // ถ้าอาหารไม่เต็ม ให้จ่ายอาหารได้ทันที
      feedAnimal(); 
    }

    countingDown = false; // รีเซ็ตสถานะการนับถอยหลังหลังจากให้อาหารเสร็จสิ้น
    delay(3000); // รอให้แสดงผลครบ 3 วินาที
    lcd.clear();
  }

  delay(100);  // หน่วงเวลาในแต่ละรอบของ loop
}