/*
  Version Management
09.11.2026 V04 Tutorial version  
05.11.2026 V01 Initial programming
*/

const char *PROGRAM_VERSION = "ESP32-C6 HLK LD2410C Human Presence Sensor Raw Parser V04";
const char *PROGRAM_VERSION_2 = "LD2410C Presence P V4";

#if defined(CONFIG_IDF_TARGET_ESP32C5) || defined(__ESP32_C5__)
// This configuration is for a display with CS Pin

// ILI9341 / ST7789 red
#define TFT_BL 7
#define TFT_CS 11

#define TFT_DC 10
#define TFT_RST 12
#define TFT_MOSI 9
#define TFT_SCLK 8

// LED Pin
//#define LED_PIN 27
#define BOOT_BTN_PIN 28         // Boot-button GPIO on your ESP32-C5 Board
const uint8_t UART_RX_PIN = 4;  // ESP32 RX <- Sensor TX (LD2420: OT1)
const uint8_t UART_TX_PIN = 3;  // ESP32 TX -> Sensor RX (RX)
const uint8_t OUT_PIN = 1;      // ESP32 TX -> OUT-Pin on Sensor (LD2420: OT2)
#define PROCESSOR_NAME "C5"

#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(__ESP32_C6__)
#define TFT_BL 0
#define TFT_CS 5

#define TFT_DC 3
#define TFT_RST 4
#define TFT_MOSI 2
#define TFT_SCLK 1

// LED Pin
#define LED_PIN 15
#define BOOT_BTN_PIN 9  // Boot-button GPIO on your ESP32-C6 Mini Board
const uint8_t UART_RX_PIN = 19;  // ESP32 RX <- Sensor TX (LD2420: OT1)
const uint8_t UART_TX_PIN = 18;  // ESP32 TX -> Sensor RX (RX)
const uint8_t OUT_PIN = 14;      // ESP32 TX -> OUT-Pin on Sensor (LD2420: OT2)
#define PROCESSOR_NAME "C6"

#else
#error "WRONG BOARD SELECTED! This sketch is supporting ESP32-C5 DevKit only boards."
#endif

// LovyanGFX Library

#include "Lov_Esp32_C5_C6_ST7789_RED_240_320.h"

LGFX_Sprite clkSprite(&tft);  // create sprite instance
bool isSpriteCreated = false;

volatile int currentBrightnessLevel = 200;

// ------------------------------------------------------
// HLK LD2410C Human Presence Sensor

#include <HardwareSerial.h>

#define MONITOR_SERIAL_SPEED 115200
#define RADAR_SERIAL_SPEED 256000

#define RADAR_RX_PIN 14   // Connect with TX Pin of the sensor
#define RADAR_TX_PIN 18   // Connect with RX Pin of the sensor
#define RADAR_OUT_PIN 19  // Connect with OUT Pin of the sensor

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
bool useDamping = true;                // Set to true to enable display damping, false for real-time
unsigned long lastDisplayUpdate = 0;   // Stores the last timestamp the OLED was refreshed
const unsigned long DAMPING_MS = 250;  // Refresh rate interval in milliseconds (4 times per second)
bool radarOutState = false;            // state of the 'OUT' GPIO

// --------------------------------
// Parser variables
uint16_t frameLen;
uint8_t dataType;
uint8_t targetStatus;
uint16_t moveDist;
uint8_t moveEnergy;
uint16_t statDist;
uint8_t statEnergy;
uint16_t detectDist;


char *DIVIDER = "------------------------------------------";

void setTftBrightness(uint8_t brightness) {
  analogWrite(TFT_BL, brightness);
  Serial.print("TFT Brightness set to ");
  Serial.println(brightness);
}

// Function to update the TFT display using a LovyanGFX sprite flackerfree
// Uses global variables directly from the parser
void updateTFTDisplay() {
    // Dimensions for the sprite in portrait mode (uses full display area)
    const int spriteWidth = 240;
    const int spriteHeight = 320; 
    
    // Initialize the sprite once on the first run
    if (!isSpriteCreated) {
        clkSprite.setColorDepth(8); // 8-bit color saves RAM on the ESP32-C6
        clkSprite.createSprite(spriteWidth, spriteHeight);
        isSpriteCreated = true;
    }

    // 1. Clear sprite (Set background to black)
    clkSprite.fillScreen(TFT_BLACK);
    
    // 2. Set text properties
    clkSprite.setTextSize(2);          // Default text size 2
    clkSprite.setTextColor(TFT_WHITE); // Default text color
    
    // Starting positions (X set to 0)
    int cursorX = 0;
    int cursorY = 0;
    int lineSpacing = 24; // Compact spacing to fit everything perfectly

    // --- Header Title (Centred with Dark Grey Background Banner) ---
    int titleHeaderHeight = 30;
    clkSprite.fillRect(0, 0, spriteWidth, titleHeaderHeight, clkSprite.color565(210, 215, 220)); // Light silver-grey banner
    
    clkSprite.setTextDatum(TC_DATUM); // Center alignment
    clkSprite.setTextColor(TFT_BLACK, clkSprite.color565(210, 215, 220)); // Dark text on light background for maximum readability
    clkSprite.drawString("LD2410C RADAR DATA", spriteWidth / 2, 8);
    
    clkSprite.setTextDatum(TL_DATUM); // Reset alignment
    cursorY += titleHeaderHeight + 6; // Optimized spacing

    // --- Report Mode & value directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Report Mode:");
    cursorY += 18;
    
    clkSprite.setCursor(cursorX, cursorY);
    if (dataType == 0x01) {
        clkSprite.setTextColor(TFT_ORANGE);
        clkSprite.println("Engineering Mode");
    } else if (dataType == 0x02) {
        clkSprite.setTextColor(TFT_GREEN);
        clkSprite.println("Normal Mode");
    } else {
        clkSprite.setTextColor(TFT_RED);
        clkSprite.println("Unknown");
    }
    cursorY += lineSpacing;

    // --- Target Status Section (With pure solid color status block on the right) ---
    uint16_t statusColor = TFT_LIGHTGRAY;
    const char* statusText = "NONE";
    
    // Determine color and short text based on targetStatus
    switch (targetStatus) {
        case 0x00: statusColor = TFT_LIGHTGRAY; statusText = "NONE"; break;
        case 0x01: statusColor = TFT_RED;       statusText = "MOVING"; break;
        case 0x02: statusColor = TFT_GREEN;     statusText = "STATIONARY"; break;
        case 0x03: statusColor = TFT_MAGENTA;   statusText = "BOTH"; break;
        default:   statusColor = TFT_YELLOW;    statusText = "UNKNOWN"; break;
    }
    
    // Draw the solid color indicator block (from "Target Status" end to screen edge, spanning two lines)
    int statusBlockX = 160; 
    int statusBlockY = cursorY - 2;
    int statusBlockWidth = spriteWidth - statusBlockX;
    int statusBlockHeight = 42; 
    clkSprite.fillRect(statusBlockX, statusBlockY, statusBlockWidth, statusBlockHeight, statusColor);
    
    // Print the static label on the left
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Target Status");
    cursorY += 18;
    
    // Print the short colored status text directly underneath (left-aligned, as requested)
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.setTextColor(statusColor); // Colored text
    clkSprite.println(statusText);
    
    cursorY += lineSpacing + 2; // Compacted to win space below

    // --- Motion Values ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Dist:   %d cm", moveDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Energy: %d %%", moveEnergy);
    cursorY += lineSpacing;

    // --- Stationary Values (Presence) ---
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Dist:   %d cm", statDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Energy: %d %%", statEnergy);
    cursorY += lineSpacing + 2; // Compacted spacing before the large highlight

    // --- Highlight: Detect Distance (Full text + centred Text Size 5 value) ---
    clkSprite.setTextColor(TFT_YELLOW);
    clkSprite.setTextSize(2);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Detect Distance (cm)");
    cursorY += 20; // Shifted 4 pixels up from previous 24

    // Center alignment for the big number
    clkSprite.setTextDatum(TC_DATUM); 
    clkSprite.setTextSize(5); // Massive size 5 for the distance value
    
    char distStr[16];
    sprintf(distStr, "%d", detectDist);
    clkSprite.drawString(distStr, spriteWidth / 2, cursorY);
    
    cursorY += 38; // Shifted 4 pixels up from previous 42 (perfectly handles text height)
    
    // Reset alignment back to top-left for the upcoming banner logic
    clkSprite.setTextDatum(TL_DATUM);
    clkSprite.setTextSize(2); // Reset to size 2

    // --- Room State Banner (Centred Text on solid Background) ---
    int rectHeight = 36;
    cursorY += 2; // Shifted 4 pixels up from previous 6 to keep it safely within 320px boundary
    
    clkSprite.setTextDatum(TC_DATUM); // Center alignment for banner text
    
    if (radarOutState) {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_RED);
        clkSprite.setTextColor(TFT_WHITE, TFT_RED);
        clkSprite.drawString("State: OCCUPIED", spriteWidth / 2, cursorY + 10);
    } else {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_GREEN);
        clkSprite.setTextColor(TFT_WHITE, TFT_GREEN);
        clkSprite.drawString("State: EMPTY", spriteWidth / 2, cursorY + 10);
    }
    
    clkSprite.setTextDatum(TL_DATUM); // Final reset

    // 3. Push the frame buffer completely flackerfree onto the TFT at (0,0)
    clkSprite.pushSprite(0, 0); 
}


// Function to update the TFT display using a LovyanGFX sprite flackerfree
// Uses global variables directly from the parser
void updateTFTDisplay_vf() {
    // Dimensions for the sprite in portrait mode (uses full display area)
    const int spriteWidth = 240;
    const int spriteHeight = 320; 
    
    // Initialize the sprite once on the first run
    if (!isSpriteCreated) {
        clkSprite.setColorDepth(8); // 8-bit color saves RAM on the ESP32-C6
        clkSprite.createSprite(spriteWidth, spriteHeight);
        isSpriteCreated = true;
    }

    // 1. Clear sprite (Set background to black)
    clkSprite.fillScreen(TFT_BLACK);
    
    // 2. Set text properties
    clkSprite.setTextSize(2);          // Default text size 2
    clkSprite.setTextColor(TFT_WHITE); // Default text color
    
    // Starting positions (X set to 0)
    int cursorX = 0;
    int cursorY = 0;
    int lineSpacing = 24; // Compact spacing to fit everything perfectly

    // --- Header Title (Centred with Dark Grey Background Banner) ---
    int titleHeaderHeight = 30;
    clkSprite.fillRect(0, 0, spriteWidth, titleHeaderHeight, clkSprite.color565(40, 40, 40)); // Dark grey banner
    
    clkSprite.setTextDatum(TC_DATUM); // Center alignment
    clkSprite.setTextColor(TFT_CYAN, clkSprite.color565(40, 40, 40));
    clkSprite.drawString("LD2410C RADAR DATA", spriteWidth / 2, 6);
    
    clkSprite.setTextDatum(TL_DATUM); // Reset alignment
    cursorY += titleHeaderHeight + 8;

    // --- Report Mode & value directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Report Mode:");
    cursorY += 18;
    
    clkSprite.setCursor(cursorX, cursorY);
    if (dataType == 0x01) {
        clkSprite.setTextColor(TFT_ORANGE);
        clkSprite.println("Engineering Mode");
    } else if (dataType == 0x02) {
        clkSprite.setTextColor(TFT_GREEN);
        clkSprite.println("Normal Mode");
    } else {
        clkSprite.setTextColor(TFT_RED);
        clkSprite.println("Unknown");
    }
    cursorY += lineSpacing;

    // --- Target Status Section (With pure solid color status block on the right) ---
    uint16_t statusColor = TFT_LIGHTGRAY;
    const char* statusText = "NONE";
    
    // Determine color and short text based on targetStatus
    switch (targetStatus) {
        case 0x00: statusColor = TFT_LIGHTGRAY; statusText = "NONE"; break;
        case 0x01: statusColor = TFT_RED;       statusText = "MOVING"; break;
        case 0x02: statusColor = TFT_GREEN;     statusText = "STATIONARY"; break;
        case 0x03: statusColor = TFT_MAGENTA;   statusText = "BOTH"; break;
        default:   statusColor = TFT_YELLOW;    statusText = "UNKNOWN"; break;
    }
    
    // Draw the solid color indicator block (from "Target Status" end to screen edge, spanning two lines)
    int statusBlockX = 160; 
    int statusBlockY = cursorY - 2;
    int statusBlockWidth = spriteWidth - statusBlockX;
    int statusBlockHeight = 42; 
    clkSprite.fillRect(statusBlockX, statusBlockY, statusBlockWidth, statusBlockHeight, statusColor);
    
    // Print the static label on the left
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Target Status");
    cursorY += 18;
    
    // Print the short colored status text directly underneath (left-aligned, as requested)
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.setTextColor(statusColor); // Colored text
    clkSprite.println(statusText);
    
    cursorY += lineSpacing + 4;

    // --- Motion Values ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Dist:   %d cm", moveDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Energy: %d %%", moveEnergy);
    cursorY += lineSpacing;

    // --- Stationary Values (Presence) ---
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Dist:   %d cm", statDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Energy: %d %%", statEnergy);
    cursorY += lineSpacing + 6;

    // --- Highlight: Detect Distance (Full text + centred Text Size 5 value) ---
    clkSprite.setTextColor(TFT_YELLOW);
    clkSprite.setTextSize(2);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Detect Distance (cm)");
    cursorY += 24;

    // Center alignment for the big number
    clkSprite.setTextDatum(TC_DATUM); 
    clkSprite.setTextSize(5); // Massive size 5 for the distance value
    
    char distStr[8];
    sprintf(distStr, "%d", detectDist);
    clkSprite.drawString(distStr, spriteWidth / 2, cursorY);
    
    cursorY += 42; // Height jump for TextSize 5
    
    // Reset alignment back to top-left for the upcoming banner logic
    clkSprite.setTextDatum(TL_DATUM);
    clkSprite.setTextSize(2); // Reset to size 2

    // --- Room State Banner (Centred Text on solid Background) ---
    int rectHeight = 36;
    cursorY += 6; 
    
    clkSprite.setTextDatum(TC_DATUM); // Center alignment for banner text
    
    if (radarOutState) {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_RED);
        clkSprite.setTextColor(TFT_WHITE, TFT_RED);
        clkSprite.drawString("State: OCCUPIED", spriteWidth / 2, cursorY + 10);
    } else {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_GREEN);
        clkSprite.setTextColor(TFT_WHITE, TFT_GREEN);
        clkSprite.drawString("State: EMPTY", spriteWidth / 2, cursorY + 10);
    }
    
    clkSprite.setTextDatum(TL_DATUM); // Final reset

    // 3. Push the frame buffer completely flackerfree onto the TFT at (0,0)
    clkSprite.pushSprite(0, 0); 
}


// Function to update the TFT display using a LovyanGFX sprite flackerfree
// Uses global variables directly from the parser
void updateTFTDisplay_ve() {
    // Dimensions for the sprite in portrait mode (uses full display area)
    const int spriteWidth = 240;
    const int spriteHeight = 320; 
    
    // Initialize the sprite once on the first run
    if (!isSpriteCreated) {
        clkSprite.setColorDepth(8); // 8-bit color saves RAM on the ESP32-C6
        clkSprite.createSprite(spriteWidth, spriteHeight);
        isSpriteCreated = true;
    }

    // 1. Clear sprite (Set background to black)
    clkSprite.fillScreen(TFT_BLACK);
    
    // 2. Set text properties
    clkSprite.setTextSize(2);          // Default text size 2
    clkSprite.setTextColor(TFT_WHITE); // Default text color
    
    // Starting positions (X set to 0)
    int cursorX = 0;
    int cursorY = 0;
    int lineSpacing = 24; // Compact spacing to fit the large TextSize 5 font

    // --- Title ---
    clkSprite.setTextColor(TFT_CYAN);
    clkSprite.drawString("LD2410C RADAR DATA", cursorX, cursorY);
    cursorY += lineSpacing + 4;

    // --- Report Mode & value directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Report Mode:");
    cursorY += 18;
    
    clkSprite.setCursor(cursorX, cursorY);
    if (dataType == 0x01) {
        clkSprite.setTextColor(TFT_ORANGE);
        clkSprite.println("Engineering Mode");
    } else if (dataType == 0x02) {
        clkSprite.setTextColor(TFT_GREEN);
        clkSprite.println("Normal Mode");
    } else {
        clkSprite.setTextColor(TFT_RED);
        clkSprite.println("Unknown");
    }
    cursorY += lineSpacing;

    // --- Target Status Indicator Block (Highly visible from 5m distance) ---
    int statusBlockX = 160;   // Starts right after "Target Status" text
    int statusBlockY = cursorY - 2; // Slight vertical offset alignment
    int statusBlockWidth = spriteWidth - statusBlockX; // Fills up to the right edge
    int statusBlockHeight = 44; // Spans across both text lines
    
    uint16_t statusColor = TFT_LIGHTGRAY;
    const char* statusText = "NONE";
    
    // Determine color and short text based on targetStatus
    switch (targetStatus) {
        case 0x00: statusColor = TFT_LIGHTGRAY; statusText = "NONE"; break;
        case 0x01: statusColor = TFT_RED;       statusText = "MOVING"; break;
        case 0x02: statusColor = TFT_GREEN;     statusText = "STATIONARY"; break;
        case 0x03: statusColor = TFT_MAGENTA;   statusText = "BOTH"; break;
        default:   statusColor = TFT_YELLOW;    statusText = "UNKNOWN"; break;
    }
    
    // Draw the solid color indicator block first
    clkSprite.fillRect(statusBlockX, statusBlockY, statusBlockWidth, statusBlockHeight, statusColor);
    
    // Draw the static label text on the left
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Target Status");
    cursorY += 18;
    
    // Draw the short status text directly underneath, printed over the colored block
    clkSprite.setCursor(statusBlockX + 4, cursorY); // Small padding inside the block
    clkSprite.setTextColor(TFT_WHITE, statusColor); // White text on matching status background
    clkSprite.println(statusText);
    
    // Reset background text color to transparent black for the next sections
    clkSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    cursorY += lineSpacing + 6;

    // --- Motion Values ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Dist:   %d cm", moveDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Energy: %d %%", moveEnergy);
    cursorY += lineSpacing;

    // --- Stationary Values (Presence) ---
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Dist:   %d cm", statDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Energy: %d %%", statEnergy);
    cursorY += lineSpacing + 4;

    // --- Highlight: Detect Distance (Full text + centred Text Size 5 value) ---
    clkSprite.setTextColor(TFT_YELLOW);
    clkSprite.setTextSize(2);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Detect Distance (cm)"); // Colon removed as requested
    cursorY += 24;

    // Center alignment for the big number
    clkSprite.setTextDatum(TC_DATUM); 
    clkSprite.setTextSize(5); // Massive size 5 for the distance value
    
    char distStr[8];
    sprintf(distStr, "%d", detectDist);
    clkSprite.drawString(distStr, spriteWidth / 2, cursorY);
    
    cursorY += 42; // Height jump for TextSize 5
    
    // Reset alignment back to top-left for the upcoming banner logic
    clkSprite.setTextDatum(TL_DATUM);
    clkSprite.setTextSize(2); // Reset to size 2

    // --- Room State Banner (Centred Text on solid Background) ---
    int rectHeight = 36;
    cursorY += 6; // Extra breathing room before the footer banner
    
    clkSprite.setTextDatum(TC_DATUM); // Center alignment for banner text
    
    if (radarOutState) {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_RED);
        clkSprite.setTextColor(TFT_WHITE, TFT_RED);
        clkSprite.drawString("State: OCCUPIED", spriteWidth / 2, cursorY + 10);
    } else {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_GREEN);
        clkSprite.setTextColor(TFT_WHITE, TFT_GREEN);
        clkSprite.drawString("State: EMPTY", spriteWidth / 2, cursorY + 10);
    }
    
    clkSprite.setTextDatum(TL_DATUM); // Final reset

    // 3. Push the frame buffer completely flackerfree onto the TFT at (0,0)
    clkSprite.pushSprite(0, 0); 
}

// Function to update the TFT display using a LovyanGFX sprite flackerfree
// Uses global variables directly from the parser
void updateTFTDisplay_vd() {
    // Dimensions for the sprite in portrait mode (uses full display area)
    const int spriteWidth = 240;
    const int spriteHeight = 320; 
    
    // Initialize the sprite once on the first run
    if (!isSpriteCreated) {
        clkSprite.setColorDepth(8); // 8-bit color saves RAM on the ESP32-C6
        clkSprite.createSprite(spriteWidth, spriteHeight);
        isSpriteCreated = true;
    }

    // 1. Clear sprite (Set background to black)
    clkSprite.fillScreen(TFT_BLACK);
    
    // 2. Set text properties
    clkSprite.setTextSize(2);          // Default text size 2
    clkSprite.setTextColor(TFT_WHITE); // Default text color
    
    // Starting positions (X set to 0)
    int cursorX = 0;
    int cursorY = 0;
    int lineSpacing = 24; // Compact spacing to fit the large TextSize 4 font

    // --- Title ---
    clkSprite.setTextColor(TFT_CYAN);
    clkSprite.drawString("LD2410C RADAR DATA", cursorX, cursorY);
    cursorY += lineSpacing + 4;

    // --- Report Mode & value directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Report Mode:");
    cursorY += 18;
    
    clkSprite.setCursor(cursorX, cursorY);
    if (dataType == 0x01) {
        clkSprite.setTextColor(TFT_ORANGE);
        clkSprite.println("Engineering Mode");
    } else if (dataType == 0x02) {
        clkSprite.setTextColor(TFT_GREEN);
        clkSprite.println("Normal Mode");
    } else {
        clkSprite.setTextColor(TFT_RED);
        clkSprite.println("Unknown");
    }
    cursorY += lineSpacing;

    // --- Target Status & state directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Target Status:");
    cursorY += 18;
    
    clkSprite.setCursor(cursorX, cursorY);
    switch (targetStatus) {
        case 0x00: clkSprite.setTextColor(TFT_LIGHTGRAY); clkSprite.println("No target detected"); break;
        case 0x01: clkSprite.setTextColor(TFT_RED);       clkSprite.println("MOVING target"); break;
        case 0x02: clkSprite.setTextColor(TFT_GREEN);     clkSprite.println("STATIONARY target"); break;
        case 0x03: clkSprite.setTextColor(TFT_MAGENTA);   clkSprite.println("BOTH targets"); break;
        default:   clkSprite.setTextColor(TFT_YELLOW);    clkSprite.println("Unknown status"); break;
    }
    cursorY += lineSpacing;

    // --- Motion Values ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Dist:   %d cm", moveDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Energy: %d %%", moveEnergy);
    cursorY += lineSpacing;

    // --- Stationary Values (Presence) ---
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Dist:   %d cm", statDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Energy: %d %%", statEnergy);
    cursorY += lineSpacing + 4;

    // --- Highlight: Detect Distance (Full text + centred Text Size 4 value) ---
    clkSprite.setTextColor(TFT_YELLOW);
    clkSprite.setTextSize(2);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Detect Distance (cm)");
    cursorY += 22;

    // Center alignment for the big number
    clkSprite.setTextDatum(TC_DATUM); 
    clkSprite.setTextSize(5); // Massive size 5 for the distance value
    
    // Convert int to string for drawString
    char distStr[8];
    sprintf(distStr, "%d", detectDist);
    clkSprite.drawString(distStr, spriteWidth / 2, cursorY);
    
    cursorY += 36; // Height jump for TextSize 4
    
    // Reset alignment back to top-left for the upcoming banner logic
    clkSprite.setTextDatum(TL_DATUM);
    clkSprite.setTextSize(2); // Reset to size 2

    // --- Room State Banner (Centred Text on solid Background) ---
    int rectHeight = 36;
    cursorY += 10; // Extra breathing room before the footer banner
    
    clkSprite.setTextDatum(TC_DATUM); // Center alignment for banner text
    
    if (radarOutState) {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_RED);
        clkSprite.setTextColor(TFT_WHITE, TFT_RED);
        clkSprite.drawString("State: OCCUPIED", spriteWidth / 2, cursorY + 10);
    } else {
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_GREEN);
        clkSprite.setTextColor(TFT_WHITE, TFT_GREEN);
        clkSprite.drawString("State: EMPTY", spriteWidth / 2, cursorY + 10);
    }
    
    clkSprite.setTextDatum(TL_DATUM); // Final reset

    // 3. Push the frame buffer completely flackerfree onto the TFT at (0,0)
    clkSprite.pushSprite(0, 0); 
}


// Function to update the TFT display using a LovyanGFX sprite flackerfree
// Uses global variables directly from the parser
void updateTFTDisplay_vb() {
    // Dimensions for the sprite in portrait mode (uses full display area)
    const int spriteWidth = 240;
    const int spriteHeight = 320; 
    
    // Initialize the sprite once on the first run
    if (!isSpriteCreated) {
        clkSprite.setColorDepth(8); // 8-bit color saves RAM on the ESP32-C6
        clkSprite.createSprite(spriteWidth, spriteHeight);
        isSpriteCreated = true;
    }

    // 1. Clear sprite (Set background to black)
    clkSprite.fillScreen(TFT_BLACK);
    
    // 2. Set text properties
    clkSprite.setTextSize(2);          // Default text size 2
    clkSprite.setTextColor(TFT_WHITE); // Default text color
    
    // Starting positions (X set to 0)
    int cursorX = 0;
    int cursorY = 0;
    int lineSpacing = 24; // Slightly tightened spacing to gain vertical room

    // --- Title ---
    clkSprite.setTextColor(TFT_CYAN);
    clkSprite.drawString("LD2410C RADAR DATA", cursorX, cursorY);
    cursorY += lineSpacing + 4;

    // --- Report Mode & value directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Report Mode:");
    cursorY += 18; // Tight jump for value
    
    clkSprite.setCursor(cursorX, cursorY);
    if (dataType == 0x01) {
        clkSprite.setTextColor(TFT_ORANGE);
        clkSprite.println("Engineering Mode");
    } else if (dataType == 0x02) {
        clkSprite.setTextColor(TFT_GREEN);
        clkSprite.println("Normal Mode");
    } else {
        clkSprite.setTextColor(TFT_RED);
        clkSprite.println("Unknown");
    }
    cursorY += lineSpacing;

    // --- Target Status & state directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Target Status:");
    cursorY += 18; // Tight jump for value
    
    clkSprite.setCursor(cursorX, cursorY);
    switch (targetStatus) {
        case 0x00: clkSprite.setTextColor(TFT_LIGHTGRAY); clkSprite.println("No target detected"); break;
        case 0x01: clkSprite.setTextColor(TFT_RED);       clkSprite.println("MOVING target"); break;
        case 0x02: clkSprite.setTextColor(TFT_GREEN);     clkSprite.println("STATIONARY target"); break;
        case 0x03: clkSprite.setTextColor(TFT_MAGENTA);   clkSprite.println("BOTH targets"); break;
        default:   clkSprite.setTextColor(TFT_YELLOW);    clkSprite.println("Unknown status"); break;
    }
    cursorY += lineSpacing;

    // --- Motion Values ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Dist:   %d cm", moveDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Energy: %d %%", moveEnergy);
    cursorY += lineSpacing;

    // --- Stationary Values (Presence) ---
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Dist:   %d cm", statDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Energy: %d %%", statEnergy);
    cursorY += lineSpacing + 4; // Buffer before the big highlight

    // --- Highlight: Detect Distance (Text Size 3) ---
    clkSprite.setTextColor(TFT_YELLOW);
    clkSprite.setTextSize(2);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Detect Dist:");
    cursorY += 20;

    clkSprite.setTextSize(3); // Scale up for the actual numeric value
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("%d", detectDist);
    cursorY += 26; // Room for the larger font height

    // Maßeinheit in der Zeile darunter
    clkSprite.setTextSize(2); // Back to size 2
    clkSprite.setTextColor(TFT_LIGHTGRAY);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("(cm)");
    cursorY += lineSpacing + 12; // Push down to the final banner

    // --- Room State Banner (Centred Text on solid Background) ---
    int rectHeight = 36; // Ideal height for TextSize 2
    
    // Set standard alignment for easy centering via LovyanGFX
    clkSprite.setTextDatum(TC_DATUM); // Top-Center alignment
    
    if (radarOutState) {
        // Draw RED rectangle for OCCUPIED
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_RED);
        clkSprite.setTextColor(TFT_WHITE, TFT_RED);
        // drawString automatically calculates center using TC_DATUM
        clkSprite.drawString("State: OCCUPIED", spriteWidth / 2, cursorY + 10);
    } else {
        // Draw GREEN rectangle for EMPTY
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_GREEN);
        clkSprite.setTextColor(TFT_WHITE, TFT_GREEN);
        clkSprite.drawString("State: EMPTY", spriteWidth / 2, cursorY + 10);
    }
    
    // Reset text datum back to top-left so it doesn't break future text cycles
    clkSprite.setTextDatum(TL_DATUM);

    // 3. Push the frame buffer completely flackerfree onto the TFT at (0,0)
    clkSprite.pushSprite(0, 0); 
}


// Function to update the TFT display using a LovyanGFX sprite flackerfree
// Uses global variables directly from the parser
void updateTFTDisplayVa() {
    // Dimensions for the sprite in portrait mode (uses full display area)
    const int spriteWidth = 240;
    const int spriteHeight = 320; 
    
    // Initialize the sprite once on the first run
    if (!isSpriteCreated) {
        clkSprite.setColorDepth(8); // 8-bit color saves RAM on the ESP32-C6
        clkSprite.createSprite(spriteWidth, spriteHeight);
        isSpriteCreated = true;
    }

    // 1. Clear sprite (Set background to black)
    clkSprite.fillScreen(TFT_BLACK);
    
    // 2. Set text properties
    clkSprite.setTextSize(2);          // Text size 2 as requested
    clkSprite.setTextColor(TFT_WHITE); // Default text color
    
    // Starting positions (X set to 0)
    int cursorX = 0;
    int cursorY = 0;
    int lineSpacing = 28; // Optimal spacing for TextSize 2 in a 240x320 layout

    // --- Title ---
    clkSprite.setTextColor(TFT_CYAN);
    clkSprite.drawString("LD2410C RADAR DATA", cursorX, cursorY);
    cursorY += lineSpacing + 6;

    // --- Report Mode & value directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Report Mode:");
    cursorY += 20; // Smaller jump to place the mode directly underneath
    
    clkSprite.setCursor(cursorX, cursorY);
    if (dataType == 0x01) {
        clkSprite.setTextColor(TFT_ORANGE);
        clkSprite.println("Engineering Mode");
    } else if (dataType == 0x02) {
        clkSprite.setTextColor(TFT_GREEN);
        clkSprite.println("Normal Mode");
    } else {
        clkSprite.setTextColor(TFT_RED);
        clkSprite.println("Unknown");
    }
    cursorY += lineSpacing + 4;

    // --- Target Status & state directly underneath ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.print("Target Status:");
    cursorY += 20; // Smaller jump to place the status directly underneath
    
    clkSprite.setCursor(cursorX, cursorY);
    switch (targetStatus) {
        case 0x00: clkSprite.setTextColor(TFT_LIGHTGRAY); clkSprite.println("No target detected"); break;
        case 0x01: clkSprite.setTextColor(TFT_RED);       clkSprite.println("MOVING target"); break;
        case 0x02: clkSprite.setTextColor(TFT_GREEN);     clkSprite.println("STATIONARY target"); break;
        case 0x03: clkSprite.setTextColor(TFT_MAGENTA);   clkSprite.println("BOTH targets"); break;
        default:   clkSprite.setTextColor(TFT_YELLOW);    clkSprite.println("Unknown status"); break;
    }
    cursorY += lineSpacing + 4;

    // --- Motion Values ---
    clkSprite.setTextColor(TFT_WHITE);
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Dist:   %d cm", moveDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Move Energy: %d %%", moveEnergy);
    cursorY += lineSpacing;

    // --- Stationary Values (Presence) ---
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Dist:   %d cm", statDist);
    cursorY += lineSpacing;
    
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Stat Energy: %d %%", statEnergy);
    cursorY += lineSpacing;

    // --- Detect Distance ---
    clkSprite.setCursor(cursorX, cursorY);
    clkSprite.printf("Detect Dist: %d cm", detectDist);
    cursorY += lineSpacing + 10; // Extra spacing before the status bar

    // --- Room State Banner (Full width background block with white text) ---
    int rectHeight = 35; // Height of the status bar
    
    if (radarOutState) {
        // Draw RED rectangle for OCCUPIED
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_RED);
        clkSprite.setCursor(cursorX + 6, cursorY + 8); // Slight offset inside the rectangle
        clkSprite.setTextColor(TFT_WHITE, TFT_RED);   // White text, Red background
        clkSprite.println("Room State: OCCUPIED");
    } else {
        // Draw GREEN rectangle for EMPTY
        clkSprite.fillRect(0, cursorY, spriteWidth, rectHeight, TFT_GREEN);
        clkSprite.setCursor(cursorX + 6, cursorY + 8); // Slight offset inside the rectangle
        clkSprite.setTextColor(TFT_WHITE, TFT_GREEN); // White text, Green background
        clkSprite.println("Room State: EMPTY");
    }

    // 3. Push the frame buffer completely flackerfree onto the TFT at (0,0)
    clkSprite.pushSprite(0, 0); 
}

void setup() {
  Serial.begin(MONITOR_SERIAL_SPEED);
  delay(500);
  Serial.println(PROGRAM_VERSION);

  if (!initDisplay()) {
    Serial.println("Error during display init, halting");
    while (1)
      ;
  } else {
    Serial.println("Display is initialized");
  }
  setOrientation(DISPLAY_ORIENTATION_TOP);
  setTextWrap(false);
  setTextSize(4);
  setTftBrightness(currentBrightnessLevel);

  println("");
  println("ESP32-C6");
  println("LD2410C");

  pinMode(RADAR_OUT_PIN, INPUT);
  Serial1.begin(RADAR_SERIAL_SPEED, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);

  delay(1000);
  fillScreen(TFT_BLACK);
}

// Helper function to print specific memory sections as formatted hex bytes
void printHexBytes(uint8_t *buf, size_t start, size_t numBytes) {
  for (size_t i = 0; i < numBytes; i++) {
    uint8_t b = buf[start + i];
    if (b < 0x10) Serial.print("0");
    Serial.print(b, HEX);
    Serial.print(" ");
  }
}

// Function to split, parse, and handle both OLED and Serial outputs
void parseLD2410cPacket(uint8_t *buf, size_t len, bool hardwareOut) {
  // Safety check: A standard data packet must be at least 23 bytes long
  if (len < 23) {
    return;  // Drop corrupted or incomplete frames silently to prevent log spam
  }

  // Extract all values from the buffer using correct array indices
  frameLen = buf[4] | (buf[5] << 8);
  dataType = buf[6];
  targetStatus = buf[8];
  moveDist = buf[9] | (buf[10] << 8);
  moveEnergy = buf[11];
  statDist = buf[12] | (buf[13] << 8);
  statEnergy = buf[14];
  detectDist = buf[15] | (buf[16] << 8);

  // Update the OLED Display with tuning and the hardware OUT indicator
  if (!useDamping || (millis() - lastDisplayUpdate >= DAMPING_MS)) {
    //updateDisplay(moveDist, moveEnergy, statDist, statEnergy, targetStatus, hardwareOut);
    updateTFTDisplay();
    lastDisplayUpdate = millis();
  }
  
  // Parallel Output to the Serial Monitor
  Serial.println("------------------ PARSED DATA ------------------");
  Serial.print("Field 01: Header, 4 Bytes: ");
  printHexBytes(buf, 0, 4);
  Serial.println(" | Meaning: Start of data");
  Serial.print("Field 02: Data Length, 2 Bytes: ");
  printHexBytes(buf, 4, 2);
  Serial.print(" | Value: ");
  Serial.print(frameLen);
  Serial.println(" Bytes within the frame");
  Serial.print("Field 03: Data Type, 1 Byte: ");
  printHexBytes(buf, 6, 1);
  Serial.print(" | Meaning: ");
  Serial.println(dataType == 0x02 ? "Normal Mode (0x02)" : "Unknown Mode");
  Serial.print("Field 04: Head Check, 1 Byte: ");
  printHexBytes(buf, 7, 1);
  Serial.println(" | Meaning: Protocol Data Head (0xAA)");
  Serial.print("Field 05: Target Status, 1 Byte: ");
  printHexBytes(buf, 8, 1);
  Serial.print(" | Value: 0x");
  Serial.print(targetStatus, HEX);
  Serial.print(" -> ");
  switch (targetStatus) {
    case 0x00: Serial.println("No target detected"); break;
    case 0x01: Serial.println("MOVING target detected"); break;
    case 0x02: Serial.println("STATIONARY target detected"); break;
    case 0x03: Serial.println("BOTH moving and stationary targets detected"); break;
    default: Serial.println("Unknown status"); break;
  }
  Serial.print("Field 06: Move Distance, 2 Bytes: ");
  printHexBytes(buf, 9, 2);
  Serial.print(" | Value: ");
  Serial.print(moveDist);
  Serial.println(" cm");
  Serial.print("Field 07: Move Energy, 1 Byte: ");
  printHexBytes(buf, 11, 1);
  Serial.print(" | Value: ");
  Serial.print(moveEnergy);
  Serial.println(" %");
  Serial.print("Field 08: Stat Distance, 2 Bytes: ");
  printHexBytes(buf, 12, 2);
  Serial.print(" | Value: ");
  Serial.print(statDist);
  Serial.println(" cm");
  Serial.print("Field 09: Stat Energy, 1 Byte: ");
  printHexBytes(buf, 14, 1);
  Serial.print(" | Value: ");
  Serial.print(statEnergy);
  Serial.println(" %");
  Serial.print("Field 10: Detect Distance, 2 Bytes: ");
  printHexBytes(buf, 15, 2);
  Serial.print(" | Value: ");
  Serial.print(detectDist);
  Serial.println(" cm");
  Serial.print("Field 11: Tail Check, 2 Bytes: ");
  printHexBytes(buf, 17, 2);
  Serial.println(" | Meaning: Protocol Data Tail (0x55 0x00)");
  Serial.print("Field 12: Footer, 4 Bytes: ");
  printHexBytes(buf, 19, 4);
  Serial.println(" | Meaning: End of Dataset");
  Serial.println();
  Serial.print("Status OUT Pin: ");
  Serial.println(hardwareOut == true ? "Person detected in room." : "No Person in room detected.");
  Serial.println("-------------------------------------------------\n");
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
      packetIndex++;  // Explicitly increment index
    }

    // Update footer history checker
    footerCheckBuffer = (footerCheckBuffer << 8) | incomingByte;

    // Check if the LD2410 standard footer (F8 F7 F6 F5) is reached
    if (footerCheckBuffer == 0xF8F7F6F5) {
      // Pass the packet, the collected length, and the live hardware out state
      parseLD2410cPacket(packetBuffer, packetIndex, radarOutState);

      // Reset buffer and index for the next packet
      packetIndex = 0;
      footerCheckBuffer = 0;  // Clear buffer to prevent double triggers
    }
  }
}