#include "HardwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "time.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"
#include <Adafruit_SSD1306.h>
#define OLED_Address 0x3C
Adafruit_SSD1306 display(128, 64);

#define WIFI_SSID "Chamith"
#define WIFI_PASSWORD "123456789"
#define API_KEY "AIzaSyBzDIzL5m8mk3Qa5weQsJ2TWJJf2QS2tcg"
#define USER_EMAIL "user@gmail.com"
#define USER_PASSWORD "User@123"
#define DATABASE_URL "https://dog-feeding-ded68-default-rtdb.firebaseio.com"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configg;
FirebaseJson json;

const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN = 2;
const int LOADCELL_DOUT_PIN2 = 33;
const int LOADCELL_SCK_PIN2 = 32;
#define ob 23
#define wa_led 13
#define fd_led 27
#define buzzer 26

HX711 scale;
HX711 scale2;

int qty1, qty2 = 0;
int sta1, sta3, sta2;
struct tm timeinfo;
int reading, command1, command2, command3, command4, command5, obstacle;
int lastReading;
int reading2, read1, read2;
int lastReading2;
const char* ntpServer = "pool.ntp.org";
String uid, message, historyPath, timestamp;
bool detection;

//REPLACE WITH YOUR CALIBRATION FACTOR
#define CALIBRATION_FACTOR 1008.05
#define CALIBRATION_FACTOR2 1051.41

float sensorReadingsArr[3];

const byte RXD2 = 16; // Connects to module's RX
const byte TXD2 = 17; // Connects to module's TX
HardwareSerial dfSD(1);
DFRobotDFPlayerMini player;

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}
unsigned long getTime() {
  time_t now;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}
void displayWeight1(int weight) {
  Serial.print("Weight 1 : ");
  Serial.print(weight);
  Serial.print(" ");
  Serial.println("g");
}
void displayWeight2(int weight) {
  Serial.print("Weight 2 : ");
  Serial.print(weight);
  Serial.print(" ");
  Serial.println("g");
}

void setup() {

  Serial.begin(9600);
  pinMode(ob, INPUT);
  pinMode(wa_led, OUTPUT);
  pinMode(fd_led, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(wa_led, LOW);
  digitalWrite(fd_led, LOW);
  digitalWrite(buzzer, LOW);
  dfSD.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(5000);
  if (player.begin(dfSD)) {
    Serial.println("OK");
    // Set volume to maximum (0 to 30).
    player.volume(17); //30 is very loud
  }
  else {
    Serial.println("Connecting to DFPlayer Mini failed!");
  }
  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  rtc_clk_cpu_freq_to_config(RTC_CPU_FREQ_80M, &config);
  rtc_clk_cpu_freq_set_config_fast(&config);
  Serial.println("Initializing the scale");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale2.begin(LOADCELL_DOUT_PIN2, LOADCELL_SCK_PIN2);

  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();
  scale2.set_scale(CALIBRATION_FACTOR2);
  scale2.tare();

  display.begin(SSD1306_SWITCHCAPVCC, OLED_Address);
  display.clearDisplay();

  initWiFi();
  configTime(0, 0, ntpServer);

  // Assign the api key (required)
  configg.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  configg.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  configg.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  configg.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&configg, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);
  historyPath = "/history";

}
void loop() {

  timestamp = getTime();
  obstacle = digitalRead(ob);
  cell1Read();
  cell2Read();
  readData();
  Display();

  if (sta1 == 1 && obstacle == LOW) {
    sta2 = 1;
    digitalWrite(buzzer, HIGH);
    delay(1000);
    digitalWrite(buzzer, LOW);
  }

  if ((millis() - lastTime) > timerDelay && sta1 == 1 && sta2 == 0) {
    message = "Dog motion Not Detected";
    detection = false;
    notify();
    history();
    sta2 = 0;
    sta1 = 0;
    lastTime = millis();
  } else if ((millis() - lastTime) > timerDelay && sta2 == 1) {
    message = "Dog motion Detected";
    detection = true;
    notify();
    history();
    sta2 = 0;
    sta1 = 0;
    lastTime = millis();
  }

}
void cell1Read() {
  if (scale.wait_ready_timeout(200)) {
    reading = round(scale.get_units());
    if (reading != lastReading) {
      displayWeight1(reading);
      read1 = map(reading, 0, 500, 0, 100);
      Firebase.RTDB.setInt(&fbdo, "/live_data/food-level", read1);
      if (read1 < 50) {
        digitalWrite(fd_led, HIGH);
      } else {
        digitalWrite(fd_led, LOW);
      }
    }
    lastReading = reading;
  }
  else {
    Serial.println("HX711 1 not found.");
  }
}
void cell2Read() {
  if (scale2.wait_ready_timeout(200)) {
    reading2 = round(scale2.get_units());
    if (reading2 != lastReading2) {
      displayWeight2(reading2);
      read2 = map(reading2, 0, 500, 0, 100);
      Firebase.RTDB.setInt(&fbdo, "/live_data/water-level", read2);
      if (read2 < 50) {
        digitalWrite(wa_led, HIGH);
      } else {
        digitalWrite(wa_led, LOW);
      }
    }
    lastReading2 = reading2;
  }
  else {
    Serial.println("HX711 2 not found.");
  }
}
void readData() {
  if (Firebase.RTDB.getBool(&fbdo, "/live_data/dog-command")) {
    if (fbdo.dataType() == "boolean") {
      command1 = fbdo.boolData();
      Serial.print("command1 : ");
      Serial.println(command1);
      if (command1 == 1) {
        sta1 = 1;
        lastTime = millis();
        player.play(1);
        Firebase.RTDB.setBool(&fbdo, "/live_data/dog-command", false);
      }
    }
  }
  if (Firebase.RTDB.getBool(&fbdo, "/live_data/sound1")) {
    if (fbdo.dataType() == "boolean") {
      command2 = fbdo.boolData();
      if (command2 == 1) {
        player.play(2);
        Firebase.RTDB.setBool(&fbdo, "/live_data/sound1", false);
      }
    }
  }
  if (Firebase.RTDB.getBool(&fbdo, "/live_data/sound2")) {
    if (fbdo.dataType() == "boolean") {
      command3 = fbdo.boolData();
      if (command3 == 1) {
        player.play(3);
        Firebase.RTDB.setBool(&fbdo, "/live_data/sound2", false);
      }
    }
  }
  if (Firebase.RTDB.getBool(&fbdo, "/live_data/sound3")) {
    if (fbdo.dataType() == "boolean") {
      command4 = fbdo.boolData();
      if (command4 == 1) {
        player.play(4);
        Firebase.RTDB.setBool(&fbdo, "/live_data/sound3", false);
      }
    }
  }
  if (Firebase.RTDB.getBool(&fbdo, "/live_data/sound4")) {
    if (fbdo.dataType() == "boolean") {
      command5 = fbdo.boolData();
      if (command5 == 1) {
        player.play(5);
        Firebase.RTDB.setBool(&fbdo, "/live_data/sound4", false);
      }
    }
  }
}
void notify() {
  Firebase.RTDB.setString(&fbdo, "/notification/message", message);
  delay(200);
  Firebase.RTDB.setBool(&fbdo, "/notification/istrue", true);
  delay(200);
}
void history() {
  String histry = historyPath + "/" + String(timestamp) + "000";
  json.set("/dog-detection", detection);
  json.set("/food-level", read1);
  json.set("/water-level", read2);
  json.set("/timestamp", timestamp.toInt());
  Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, histry.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
}
void Display() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(25, 0);
  display.println("Dog Feeding");
  display.setCursor(0, 20);
  display.print("Dog Detection : ");
  display.println(detection);
  display.print("Food Level : ");
  display.print(read1);
  display.println(" %");
  display.print("Water Level : ");
  display.print(read2);
  display.println(" %");
  display.display();
}
