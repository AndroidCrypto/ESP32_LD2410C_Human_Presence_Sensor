/*
  Version Management
05.11.2026 V01 Initial programming
*/

const char *PROGRAM_VERSION = "ESP32-C6 HLK LD2410C Human Presence Sensor Raw Record Limiter V01";

// ------------------------------------------------------
// HLK LD2410C Human Presence Sensor

#include <HardwareSerial.h>

#define MONITOR_SERIAL_SPEED 115200
#define RADAR_SERIAL_SPEED 256000

#define RADAR_RX_PIN 14  // Connect with TX Pin of the sensor
#define RADAR_TX_PIN 18  // Connect with RX Pin of the sensor

// Buffer to detect the 4-byte footer: F8 F7 F6 F5
uint32_t footerCheckBuffer = 0;

void setup() {
  Serial.begin(MONITOR_SERIAL_SPEED);
  delay(500);
  Serial.println(PROGRAM_VERSION);

  Serial1.begin(RADAR_SERIAL_SPEED, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);


  delay(1000);
}

void loop() {

  if (Serial1.available()) {
    uint8_t incomingByte = Serial1.read();

    // Print byte nicely formatted
    if (incomingByte < 0x10) {
      Serial.print("0");
    }
    Serial.print(incomingByte, HEX);
    Serial.print(" ");

    // Slide the incoming byte into our 32-bit footer history checker
    footerCheckBuffer = (footerCheckBuffer << 8) | incomingByte;

    // Check if the last 4 bytes match the LD2410 standard footer: F8 F7 F6 F5
    if (footerCheckBuffer == 0xF8F7F6F5) {
      // Footer detected! Break line and mark packet boundary
      Serial.println(" <- [PACKET END]");
    }
  }
}
