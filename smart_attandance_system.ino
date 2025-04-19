
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <MFRC522.h>

// GPIO pin definitions
#define RST_PIN 5   // GPIO5 (D1)
#define SS_PIN 4    // GPIO4 (D2)
#define LED_PIN 0   // GPIO0 (D3)

const char* ssid = "iqoo z7 5g";
const char* password = "";
const char* BOT_TOKEN = "8051119470:AAHe8r_T4kw2KIIXP0RvveIqeAYMT5OPLHI";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // IST

MFRC522 rfid(SS_PIN, RST_PIN);

struct Student {
  String uid;
  String name;
  String chatId;
  bool startScan;
  bool endScan;
};

Student students[] = {
  {"73692803", "Jyoti kumari", "6216625057", false, false},
  {"8964C901", "Ritik kumar", "6216625057", false, false}
};

int studentCount = sizeof(students) / sizeof(Student);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  timeClient.begin();
}

void loop() {
  timeClient.update();
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();

  scanRFID();

  if (hour == 14 && minute == 5) {
    checkAbsentees();
    delay(60000);
  }
}

void scanRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  rfid.PICC_HaltA();

  timeClient.update();
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  String timeStr = String(hour) + ":" + (minute < 10 ? "0" : "") + String(minute);

  Serial.println("Card scanned: UID = " + uid);

  bool found = false;

  for (int i = 0; i < studentCount; i++) {
    if (students[i].uid == uid) {
      found = true;

      // 🔴 LED blink on successful card scan
      digitalWrite(LED_PIN, HIGH);
      delay(300);
      digitalWrite(LED_PIN, LOW);

      Serial.println("Student: " + students[i].name);
      Serial.println("Scan Time: " + timeStr);

      String msg = "👤 " + students[i].name + " now in class " + timeStr + ". UID: " + uid;
      sendTelegramMessage(msg, students[i].chatId);

      if ((hour == 9 && minute >= 30) || (hour == 10) || (hour == 11 && minute == 0)) {
        students[i].startScan = true;
        Serial.println("Scan Type: START");
      } else if ((hour == 13) || (hour == 14 && minute == 0)) {
        students[i].endScan = true;
        Serial.println("Scan Type: END");
      } else {
        Serial.println("⏱ Not within attendance time slot.");
      }

      Serial.println("✅ Attendance recorded.\n");
      break;
    }
  }

  if (!found) {
    Serial.println("⛔ Unknown card - not registered.\n");
  }
}

void checkAbsentees() {
  for (int i = 0; i < studentCount; i++) {
    if (!students[i].startScan && !students[i].endScan) {
      String msg = "🚫 " + students[i].name + " was ABSENT today (missed both scans).";
      sendTelegramMessage(msg, students[i].chatId);
    }

    students[i].startScan = false;
    students[i].endScan = false;
  }
}

void sendTelegramMessage(String msg, String chatId) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) +
                 "/sendMessage?chat_id=" + chatId +
                 "&text=" + msg;

    https.begin(client, url);
    int httpCode = https.GET();

    if (httpCode > 0) {
      Serial.println("📨 Telegram message sent to " + chatId);
    } else {
      Serial.println("⚠️ Telegram send failed.");
    }

    https.end();
  }
}
