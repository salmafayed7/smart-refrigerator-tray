// Blynk info
#define BLYNK_TEMPLATE_ID "TMPL2V_0Xx4m7"
#define BLYNK_TEMPLATE_NAME "Refrigerator Tray"
#define BLYNK_AUTH_TOKEN "ICVp5uCaCj8FLTFO0eS7Tl5xGXeuRRuC"

// libraries
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "HX711.h"
#include <LiquidCrystal.h>

// Wi-Fi credentials
char ssid[] = "net";
char pass[] = "whatever";

// HX711 settings
#define DOUT1 4   // amplifer data out pin connected to GPIO 4
#define SCK1 5    // amplifier serial clock pin connected to GPIO 5
HX711 scale1;

// item settings
#define ITEM_WEIGHT 225.0     // weight of one item in grams
#define RESTOCK_THRESHOLD 2     // alert when restock is needed
#define NOISE_THRESHOLD 10.0    // ignore changes smaller than this
#define ZERO_THRESHOLD 20.0     // treat values below this as zero
#define DOOR_SENSOR_PIN  19     // door sensor pin connected to GPIO 19


// LCD pins (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(33, 32, 25, 26, 27, 14);


BlynkTimer timer;
float last_weight = 0;
int item_count = 0;
bool restockNotified = false;
bool scale_ready = false;
int doorState;
bool isDoorOpen = false;
bool toggle = false;
bool doorNotified = false;
float calibration_factor = 415.29151515151515151515151515151515151515151515151515151515;  

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP); 

  // setup LCD
  lcd.begin(16, 2);
  lcd.print("Initializing...");

  // setup scale
  scale1.begin(DOUT1, SCK1);
  Serial.println("Waiting for scale to be ready...");
  while (!scale1.is_ready()) {
    delay(100);
  }
  scale1.set_scale(calibration_factor);
  delay(2000);  // delay for stabilization

  // get average of 10 readings
  float rawWeight = scale1.get_units(10);

  // if current weight is below 175 (no chocolate mix on the scale), tare
  if (abs(rawWeight) < 175.0) {  
    scale1.tare();
    Serial.println("Scale tared (tray empty)");
  }
  else {
   Serial.println("Tray not empty — skipping tare");
  } 
  scale_ready = true;
  Serial.println("Scale ready");


  // connect to Wi-Fi
  lcd.clear();
  lcd.print("Connecting WiFi");

  WiFi.setHostname("ESP32-FridgeTray");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
    if (wifiTimeout > 20) {
      Serial.println("WiFi Failed!");
      lcd.clear();
      lcd.print("WiFi Failed!");
      return;
    }
  }

  Serial.println("WiFi connected");
  lcd.clear();
  lcd.print("WiFi OK");

  // connect to Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  int blynkTimeout = 0;
  while (!Blynk.connected()) {
    Blynk.run();
    delay(500);
    Serial.print("#");
    blynkTimeout++;
    if (blynkTimeout > 20) {
      Serial.println("Blynk Failed!");
      lcd.clear();
      lcd.print("Blynk Failed!");
      return;
    }
  }

  Serial.println("Blynk connected");


  lcd.clear();
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();

  // perform weight & door checks every 2 seconds
  timer.setInterval(2000L, checkWeight);
  timer.setInterval(2000L, checkDoor);
  Serial.println("Setup complete");
}

void checkWeight() {
  if (!scale_ready || !scale1.is_ready()) {
    Serial.println("Scale not ready");
    return;
  }

  float current_weight = scale1.get_units(10); 

  // if current weight is below zero threshold, consider the weight 0
  if (abs(current_weight) < ZERO_THRESHOLD) {
    current_weight = 0;
  }


  // if weight detected is greater than noise threshold, increase item count
  if (abs(current_weight - last_weight) > NOISE_THRESHOLD) {
    last_weight = current_weight;
    item_count = max(0, (int)round(current_weight / ITEM_WEIGHT));


    // send to Blynk
    Blynk.virtualWrite(V0, last_weight);
    Blynk.virtualWrite(V1, item_count);
    Blynk.virtualWrite(V2, item_count < RESTOCK_THRESHOLD ? 2 : 0);
    
    Serial.print("Weight: ");
    Serial.print(current_weight);
    Serial.print("g, Count: ");
    Serial.print(item_count);
    Serial.println(" items");

    notifyRestock();
  }
  updateLCD();
}

void checkDoor(){
  doorState = digitalRead(DOOR_SENSOR_PIN); // read door state 
    if (doorState == HIGH) {
    Serial.println("The door is closed");
    isDoorOpen = false;
  }
  else {
    Serial.println("The door is open");
    isDoorOpen = true;
    Blynk.virtualWrite(V3, isDoorOpen);
    toggle = true;
    updateLCD();
  }
}

void updateLCD() {
  // print item count and weight on first row
  lcd.setCursor(0, 0);
  lcd.print("C:");
  lcd.print(item_count);
  lcd.print(" W:");
  // if measured weight is -ve, consider it 0
  if (last_weight <= 0) {
    last_weight = 0;
  }
  lcd.print(last_weight, 0);
  lcd.print("g    "); 

  // print sttaus lines on second row
  lcd.setCursor(0, 1);
  if (isDoorOpen) {
    // toggles between "Door is open!" and item count status
    if (toggle) {  
      lcd.print("Door is open!   ");
      delay(1500);
    }
    else {
      if (item_count < RESTOCK_THRESHOLD && item_count >= 0) {
        lcd.print("RESTOCK NEEDED! ");
        delay(1500);
      }
      else {
        lcd.print("Status: OK      ");
        delay(1500);
      }
    }
    toggle = !toggle;  // flip toggle for next update
  }
  else {
    // if door is closed, just show item count status
    if (item_count < RESTOCK_THRESHOLD && item_count >= 0) {
      lcd.print("RESTOCK NEEDED! ");
    }
    else {
      lcd.print("Status: OK      ");
    }
  }
}

// send Blynk notification if restock is needed
void notifyRestock(){
  if(item_count < RESTOCK_THRESHOLD)
    Blynk.logEvent("restock_needed");
    //restockNotified = true;
  }
  // else if (item_count >= RESTOCK_THRESHOLD) {
  //   restockNotified = false; 
  // }


// send Blynk notification if door is open
void notifyDoor(){
  if (isDoorOpen && !doorNotified){
    Blynk.logEvent("door_is_open");
    doorNotified = true;
  }
  else if (!isDoorOpen) {
    doorNotified = false; 
  }
}

void loop() {
  Blynk.run();
  notifyDoor();
  
  timer.run();
}
