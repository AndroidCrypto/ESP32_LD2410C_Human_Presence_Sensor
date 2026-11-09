/*
  Version Management
05.11.2026 V01 Initial programming
https://share.google/aimode/Sxj8D6MeGelclTxfY
*/

const char *PROGRAM_VERSION = "ESP32-C6 HLK LD2410C Human Presence Sensor Raw Parser V02";
const char *PROGRAM_VERSION_2 = "LD2410C Presence P V3";

// ------------------------------------------------------
// HLK LD2410C Human Presence Sensor

#include <HardwareSerial.h>

#define MONITOR_SERIAL_SPEED 115200
#define RADAR_SERIAL_SPEED 256000

#define RADAR_RX_PIN 14  // Connect with TX Pin of the sensor
#define RADAR_TX_PIN 18  // Connect with RX Pin of the sensor
#define RADAR_OUT_PIN 19   // Connect with OUT Pin of the sensor

// Non-blocking timer variables (2000 ms = 2 seconds)
unsigned long previousMillis = 0;
const long interval = 2000;

// Buffer to detect the 4-byte footer: F8 F7 F6 F5
uint32_t footerCheckBuffer = 0;

// Define the maximum packet size (Normal Mode requires exactly 23 bytes)
#define MAX_PACKET_SIZE 30
uint8_t packetBuffer[MAX_PACKET_SIZE];
size_t packetIndex = 0;

// --- NEW TUNING CONFIGURATION ---
bool useDamping = true;               // Set to true to enable display damping, false for real-time
unsigned long lastDisplayUpdate = 0;  // Stores the last timestamp the OLED was refreshed
const unsigned long DAMPING_MS = 250; // Refresh rate interval in milliseconds (4 times per second)
bool radarOutState = false;  // state of the 'OUT' GPIO

// --------------------------------
// Parser variables
uint16_t frameLen;
  uint8_t dataType;
  uint8_t  targetStatus;
  uint16_t moveDist;
  uint8_t  moveEnergy;
  uint16_t statDist;
  uint8_t  statEnergy;
  uint16_t detectDist;


char *DIVIDER = "------------------------------------------";

void setup() {
  Serial.begin(MONITOR_SERIAL_SPEED);
  delay(500);
  Serial.println(PROGRAM_VERSION);

  pinMode(RADAR_OUT_PIN, INPUT);

  Serial1.begin(RADAR_SERIAL_SPEED, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
    
  delay(1000);
}

// Function to split, parse, and handle both OLED and Serial outputs
void parseLD2410cPacket(uint8_t* buf, size_t len, bool hardwareOut) {
  // Safety check: A standard data packet must be at least 23 bytes long
  if (len < 23) {
    return; // Drop corrupted or incomplete frames silently to prevent log spam
  }

  // Extract all values from the buffer using correct array indices
  frameLen   = buf[4] | (buf[5] << 8);
  dataType = buf[6];
  targetStatus = buf[8];
  moveDist   = buf[9] | (buf[10] << 8);
  moveEnergy = buf[11];
  statDist   = buf[12] | (buf[13] << 8);
  statEnergy = buf[14];
  detectDist = buf[15] | (buf[16] << 8);

  // Update the OLED Display with tuning and the hardware OUT indicator
  if (!useDamping || (millis() - lastDisplayUpdate >= DAMPING_MS)) {
    //updateDisplay(moveDist, moveEnergy, statDist, statEnergy, targetStatus, hardwareOut);
    lastDisplayUpdate = millis();
  }

  // Parallel Output to the Serial Monitor
  Serial.println("------------------ PARSED DATA ------------------");
  Serial.print("Field 01: Header, 4 Bytes: "); printHexBytes(buf, 0, 4); Serial.println(" | Meaning: Start of data");
  Serial.print("Field 02: Data Length, 2 Bytes: "); printHexBytes(buf, 4, 2); Serial.print(" | Value: "); Serial.print(frameLen); Serial.println(" Bytes within the frame");
  Serial.print("Field 03: Data Type, 1 Byte: "); printHexBytes(buf, 6, 1); Serial.print(" | Meaning: "); Serial.println(dataType == 0x02 ? "Normal Mode (0x02)" : "Unknown Mode");
  Serial.print("Field 04: Head Check, 1 Byte: "); printHexBytes(buf, 7, 1); Serial.println(" | Meaning: Protocol Data Head (0xAA)");
  Serial.print("Field 05: Target Status, 1 Byte: "); printHexBytes(buf, 8, 1); Serial.print(" | Value: 0x"); Serial.print(targetStatus, HEX); Serial.print(" -> ");
  switch (targetStatus) {
    case 0x00: Serial.println("No target detected"); break;
    case 0x01: Serial.println("MOVING target detected"); break;
    case 0x02: Serial.println("STATIONARY target detected"); break;
    case 0x03: Serial.println("BOTH moving and stationary targets detected"); break;
    default:   Serial.println("Unknown status"); break;
  }
  Serial.print("Field 06: Move Distance, 2 Bytes: "); printHexBytes(buf, 9, 2); Serial.print(" | Value: "); Serial.print(moveDist); Serial.println(" cm");
  Serial.print("Field 07: Move Energy, 1 Byte: "); printHexBytes(buf, 11, 1); Serial.print(" | Value: "); Serial.print(moveEnergy); Serial.println(" %");
  Serial.print("Field 08: Stat Distance, 2 Bytes: "); printHexBytes(buf, 12, 2); Serial.print(" | Value: "); Serial.print(statDist); Serial.println(" cm");
  Serial.print("Field 09: Stat Energy, 1 Byte: "); printHexBytes(buf, 14, 1); Serial.print(" | Value: "); Serial.print(statEnergy); Serial.println(" %");
  Serial.print("Field 10: Detect Distance, 2 Bytes: "); printHexBytes(buf, 15, 2); Serial.print(" | Value: "); Serial.print(detectDist); Serial.println(" cm");
  Serial.print("Field 11: Tail Check, 2 Bytes: "); printHexBytes(buf, 17, 2); Serial.println(" | Meaning: Protocol Data Tail (0x55 0x00)");
  Serial.print("Field 12: Footer, 4 Bytes: "); printHexBytes(buf, 19, 4); Serial.println(" | Meaning: End of Dataset");
  Serial.println();
  Serial.print("Status OUT Pin: "); Serial.println(hardwareOut == true ? "Person detected in room." : "No Person in room detected.");
  Serial.println("-------------------------------------------------\n");
}

// Helper function to print specific memory sections as formatted hex bytes
void printHexBytes(uint8_t* buf, size_t start, size_t numBytes) {
  for (size_t i = 0; i < numBytes; i++) {
    uint8_t b = buf[start + i];
    if (b < 0x10) Serial.print("0");
    Serial.print(b, HEX);
    Serial.print(" ");
  }
}

void loop() {
  // 1. Read the hardware OUT pin state and check for state changes smoothly
  bool currentOut = digitalRead(RADAR_OUT_PIN);
  if (currentOut != radarOutState) {
    radarOutState = currentOut;
    if (radarOutState) {
      Serial.println("\n>>> HARDWARE OUT: DETECTED <<<");
    } else {
      Serial.println("\n>>> HARDWARE OUT: CLEAR <<<");
    }
  }

  // 2. Read incoming serial bytes from the LD2410 sensor
  while (Serial1.available() > 0) {
    uint8_t incomingByte = Serial1.read();

    // Store byte in the buffer for later processing
    if (packetIndex < MAX_PACKET_SIZE) {
      packetBuffer[packetIndex] = incomingByte;
      packetIndex++; // Explicitly increment index
    }

    // Update footer history checker
    footerCheckBuffer = (footerCheckBuffer << 8) | incomingByte;

    // Check if the LD2410 standard footer (F8 F7 F6 F5) is reached
    if (footerCheckBuffer == 0xF8F7F6F5) {
      // Pass the packet, the collected length, and the live hardware out state
      parseLD2410cPacket(packetBuffer, packetIndex, radarOutState);
      
      // Reset buffer and index for the next packet
      packetIndex = 0;
      footerCheckBuffer = 0; // Clear buffer to prevent double triggers
    }
  }
}