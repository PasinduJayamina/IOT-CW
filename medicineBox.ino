#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// --- OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Firebase Config ---
#define FIREBASE_HOST "smart-care-hub-a7564-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "Qd6unLhJUtwFYeQfsowhq5zcxxB63FHxDV9QX1tP"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- WiFi ---
#define WIFI_SSID "Pdj"
#define WIFI_PASSWORD "12345678a"

// --- NTP ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // GMT+5:30

// --- Pins ---
#define AFTERNOON_PIN D5
#define NIGHT_PIN D6

// --- State Vars ---
bool afternoonTaken = false;
bool nightTaken = false;

int lastAfternoonState = HIGH;
int lastNightState = HIGH;

unsigned long lastAfternoonChange = 0;
unsigned long lastNightChange = 0;
const unsigned long debounceMs = 50;

// Reset testing: 2 min
unsigned long lastResetTime = 0;
const unsigned long resetInterval = 120000; // 2 minutes

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void setup() {
  Serial.begin(9600);
  pinMode(AFTERNOON_PIN, INPUT_PULLUP);
  pinMode(NIGHT_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  connectWiFi();

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
  timeClient.update();

  updateOLED("Smart MedBox", "Starting...");
  delay(1000);
}

void loop() {
  timeClient.update();
  unsigned long nowMs = millis();

  // --- Afternoon ---
  int currentAfternoonState = digitalRead(AFTERNOON_PIN);
  if (currentAfternoonState != lastAfternoonState) {
    if (nowMs - lastAfternoonChange > debounceMs) {
      if (currentAfternoonState == HIGH) {       // Drawer OPENED
        if (!afternoonTaken) {
          updateStatus("afternoon", true);
          afternoonTaken = true;
        }
      }
      // Ignore CLOSE for Taken/Pending logic
      lastAfternoonChange = nowMs;
      lastAfternoonState = currentAfternoonState;
    }
  }

  // --- Night ---
  int currentNightState = digitalRead(NIGHT_PIN);
  if (currentNightState != lastNightState) {
    if (nowMs - lastNightChange > debounceMs) {
      if (currentNightState == HIGH) {           // Drawer OPENED
        if (!nightTaken) {
          updateStatus("night", true);
          nightTaken = true;
        }
      }
      // Ignore CLOSE
      lastNightChange = nowMs;
      lastNightState = currentNightState;
    }
  }

  // --- Timed Reset ---
  if (nowMs - lastResetTime >= resetInterval) {
    lastResetTime = nowMs;
    resetAllStatuses();
  }

  displayStatus();
  delay(20); // responsive
}

void updateStatus(const String& timeSlot, bool isTaken) {
  String path = "/users/USER_ID_1/medicineBox/" + timeSlot;
  Firebase.setBool(fbdo, path + "/isTaken", isTaken);
  Firebase.setInt(fbdo, path + "/lastTakenTimestamp", timeClient.getEpochTime());
}

void resetAllStatuses() {
  Serial.println("Reset triggered.");
  updateStatus("afternoon", false);
  updateStatus("night", false);
  Firebase.setString(fbdo, "/users/USER_ID_1/medicineBox/morning/missed", "Hardware issue");
  afternoonTaken = false;
  nightTaken = false;
}

void displayStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Morning: [DISABLED]");

  display.setCursor(0, 15);
  display.print("Afternoon: ");
  display.println(afternoonTaken ? "Taken" : "Pending");

  display.setCursor(0, 30);
  display.print("Night: ");
  display.println(nightTaken ? "Taken" : "Pending");

  display.setCursor(0, 50);
  display.print("Next reset in: ");
  unsigned long msLeft = (resetInterval - (millis() - lastResetTime)) / 1000;
  display.print(msLeft);
  display.println("s");

  display.display();
}

void updateOLED(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 15);
  display.println(line2);
  display.display();
}
