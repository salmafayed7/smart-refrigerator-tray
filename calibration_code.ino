#include <HX711.h>
#include <LiquidCrystal.h>

// HX711 pins
#define DT 4
#define SCK 5

// LCD pins (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(33, 32, 25, 26, 27, 14);

HX711 scale;

// Calibration factor (from your test)
// #define CALIBRATION_FACTOR 209.52 //

void setup() {
  Serial.begin(115200);

  // LCD setup
  lcd.begin(16, 2);
  lcd.print("Initializing...");
  
  // HX711 setup
  scale.begin(DT, SCK);
  scale.set_scale();
  scale.tare();  // Set the current reading to 0

  delay(2000);
  lcd.clear();
  lcd.print("Ready");
  delay(1000);
}

void loop() {
  float weight = scale.get_units(20); // Average of 10 readings

  lcd.clear();

  // Display weight
  lcd.setCursor(0, 0);
  lcd.print("Weight:");
  lcd.setCursor(8, 0);
  lcd.print(weight, 1);  // One decimal place
  lcd.print(" g");

  // Detection threshold (adjustable if needed)
  if (weight > 150.0) {
    lcd.setCursor(0, 1);
    lcd.print("Object detected");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("No object      ");
  }

  delay(1000);
}
