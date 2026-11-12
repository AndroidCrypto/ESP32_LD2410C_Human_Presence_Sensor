/*
 * ==================================================================================
 * ESP32-C6 FMCW Radar Dashboard & Configuration Suite
 * Hardware: ESP32-C6 | HLK-LD2410C Presence Sensor | ST7789 TFT Display (240x320)
 * Graphics Library: LovyanGFX (Utilizing Double-Buffered Sprites for Flicker-Free UI)
 * Sensor Library: ld2410 by Nick Reynolds
 * ==================================================================================
 * 
 * DESCRIPTION:
 * This program serves as a real-time monitoring and advanced configuration interface 
 * for the HLK-LD2410C human presence sensor. It features a robust 6-way rolling menu 
 * system operated entirely via a debounced physical BOOT button. Short clicks cycle 
 * through the operational and preview menus, while long clicks (3 seconds) trigger 
 * hardware execution commands.
 * 
 * OPERATIONAL MODES:
 * 
 * 1. DISPLAY_STANDARD (Home Screen)
 *    Reads and prints a comprehensive static configuration table of the sensor. Displays 
 *    the current firmware version, dynamic max gate limits (Moving/Static), individual 
 *    gate sensitivities/range metrics (Gates 0-8), and the sensor shutdown idle timeout.
 * 
 * 2. DISPLAY_REALTIME (Live Equalizer Mode)
 *    Automatically unlocks and streams raw FMCW energy data via Engineering Mode at a 
 *    fluid 256000 Baud rate. Features a giant real-time bar graph comparing movement 
 *    energy (White/Grey) and stationary presence energy (Red/Orange) side-by-side. 
 *    Permanently overlays 2px high-contrast green lines representing active thresholds. 
 *    The bars dynamically shift colors when a threshold is breached ("detected") for 
 *    maximum visibility from a 5-meter distance.
 * 
 * 3. DISPLAY_CALIBRATION (Automated Noise Mapping)
 *    Previews a prompt to execute automated environment calibration. A 3-second hold 
 *    triggers a 10-second room exit countdown followed by a 15-second measurement phase. 
 *    The ESP32 records peak static noise profiles across all 9 gates, automatically 
 *    adds a percentage safety buffer, writes the optimized sensitivities back to the 
 *    registers, commits them to the permanent flash storage, and reboots the radar.
 * 
 * 4. DISPLAY_LOAD_CALIBRATION (Restore Saved Profiles)
 *    Provides a staging screen to flash pre-recorded, hardcoded baseline validation 
 *    arrays directly into the radar configuration registers. Overrides runtime parameters, 
 *    executes a mandatory hardware reboot to commit data to flash, and synchronizes the 
 *    local global UI threshold boundaries instantly without requiring a new manual room calibration.
 * 
 * 5. DISPLAY_SET_BASIC (Basic Range & Timeout Configuration)
 *    Displays a 3x3 comparison table contrasting active sensor parameters against new 
 *    target constants (Max Moving Gates, Max Stationary Gates, and Inactivity Timeout). 
 *    Holding the button for 3 seconds writes the updated boundary values to the sensor, 
 *    commits the configuration to flash via reboot, and returns to the home screen.
 * 
 * 6. DISPLAY_RESET (Hardware Factory Default)
 *    Acts as a secure fallback menu to wipe all customized distance sensitivity maps. 
 *    Holding the button for 3 seconds transmits a native system reset request to the 
 *    sensor chip, restores original HLK factory out-of-the-box sensitivity specifications, 
 *    reboots the peripheral core, and flushes runtime local visualization arrays.
 * 
 * ==================================================================================
 */

/*
  Version Management
12.11.2026 V01 Initial programming
*/
const char* PROGRAM_VERSION = "ESP32-C6 PCB LovyanGFX ST7789 HLK LD2410C Human Presence Sensor Advanced V01";
const char* PROGRAM_VERSION_SHORT = "ESP32-C6 HLK LD2410C";

#include <Arduino.h>
#include <SPI.h>

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
#define BOOT_BTN_PIN 28  // Boot-button GPIO on your ESP32-C5 Board
#define RADAR_RX_PIN 1   // Connect with TX Pin of the sensor
#define RADAR_TX_PIN 3   // Connect with RX Pin of the sensor
#define RADAR_OUT_PIN 4  // Connect with OUT Pin of the sensor
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
#define BOOT_BTN_PIN 9    // Boot-button GPIO on your ESP32-C6 Mini Board
#define RADAR_RX_PIN 14   // Connect with TX Pin of the sensor
#define RADAR_TX_PIN 18   // Connect with RX Pin of the sensor
#define RADAR_OUT_PIN 19  // Connect with OUT Pin of the sensor
#define PROCESSOR_NAME "C6"

#else
#error "WRONG BOARD SELECTED! This sketch is supporting ESP32-C5 DevKit only boards."
#endif

#include "Lov_Esp32_C5_C6_ST7789_RED_240_320.h"

#include <HardwareSerial.h>
#include <ld2410.h>  // https://github.com/ncmreynolds/ld2410/ v 0.2.2

#define MONITOR_SERIAL_SPEED 115200
#define RADAR_SERIAL_SPEED 256000


// Boot button on the ESP32-C6 Super Mini is tied to GPIO 9
#define BOOT_BUTTON_PIN 9

ld2410 radar;

// LovyanGFX Sprite for flicker-free live updates in the lower section
LGFX_Sprite liveDataSprite(&tft);

// Navigation and interface state variables
// Enum for the three different display and operation modes
enum DisplayMode {
  DISPLAY_STANDARD,
  DISPLAY_REALTIME,
  DISPLAY_CALIBRATION,
  DISPLAY_LOAD_CALIBRATION,
  DISPLAY_SET_BASIC,
  DISPLAY_RESET
};

// Global state variable, replacing the old boolean
DisplayMode currentDisplayMode = DISPLAY_STANDARD;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Global arrays to store the fetched thresholds for all 9 gates (0 to 8)
uint8_t globalMoveThresholds[9] = { 0 };
uint8_t globalStatThresholds[9] = { 0 };

// Calibration data
// Living room real data
uint8_t calibrationMoveThresholds[9] = { 37, 35, 35, 31, 27, 28, 27, 32, 30 };
uint8_t calibrationStatThresholds[9] = { 15, 15, 76, 34, 27, 32, 32, 40, 80 };

// Factory
//uint8_t calibrationMoveThresholds[9] = { 50, 50, 40, 30, 20, 15, 15, 15, 15 };
//uint8_t calibrationStatThresholds[9] = { 15, 15, 40, 40, 30, 30, 20, 20, 20 };

// Target values for Basic Configuration mode
const uint8_t TARGET_MAX_MOV_GATES  = 4; // Max moving detection gate (0-8)
const uint8_t TARGET_MAX_STAT_GATES = 4; // Max stationary detection gate (0-8)
const uint16_t TARGET_TIMEOUT_SEC   = 1; // Inactivity timeout delay in seconds

// Function to fetch thresholds from the sensor and store them globally
// Now includes a copy-pasteable C++ array syntax output in the Serial Monitor
void fetchRadarThresholds() {
  Serial.println("Fetching current thresholds from LD2410...");

  // Request current configuration from the sensor
  if (radar.requestCurrentConfiguration()) {
    for (uint8_t gate = 0; gate <= 8; gate++) {
      globalMoveThresholds[gate] = radar.motion_sensitivity[gate];
      globalStatThresholds[gate] = radar.stationary_sensitivity[gate];

      Serial.printf("Fetched Gate %d -> Move Thresh: %d%% | Stat Thresh: %d%%\n",
                    gate, globalMoveThresholds[gate], globalStatThresholds[gate]);
    }
    Serial.println("Thresholds successfully stored globally.");

    // --- NEW: Copy-pasteable C++ Array Code Output ---
    Serial.println("\n================ COPY FOR YOUR CODE ================");

    // 1. Output for Moving Thresholds
    Serial.print("uint8_t calibrationMoveThresholds[9] = { ");
    for (uint8_t i = 0; i <= 8; i++) {
      Serial.print(globalMoveThresholds[i]);
      if (i < 8) Serial.print(", ");
    }
    Serial.println(" };");

    // 2. Output for Stationary Thresholds
    Serial.print("uint8_t calibrationStatThresholds[9] = { ");
    for (uint8_t i = 0; i <= 8; i++) {
      Serial.print(globalStatThresholds[i]);
      if (i < 8) Serial.print(", ");
    }
    Serial.println(" };");

    Serial.println("====================================================\n");

  } else {
    Serial.println("ERROR: Could not fetch configuration from radar. Using default 0.");
  }
}

void drawStaticUserInterface() {
  tft.fillScreen(TFT_BLACK);

  // Fetch current configuration parameters from the sensor first
  if (radar.requestCurrentConfiguration()) {
    delay(100);

    // 1. Header Section
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 5);
    tft.print(PROGRAM_VERSION_SHORT);

    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    tft.setCursor(5, 23);
    tft.print("FW: ");
    if (radar.firmware_major_version > 0) {
      tft.print(radar.firmware_major_version);
      tft.print(".");
      tft.print(radar.firmware_minor_version, HEX);
    } else {
      tft.print("Unknown");
    }

    // --- NEW: Appending Max Gates into the firmware line further right ---
    tft.setCursor(120, 23);
    tft.printf("Max M:%d S:%d", radar.max_moving_gate, radar.max_stationary_gate);

    // Separation Line
    tft.drawFastHLine(0, 35, 240, TFT_DARKGRAY);

    // 2. Table Header Drawing with Dynamic Max Gate Values over columns
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.setTextSize(1);

    // Line 1 of header: Print the dynamic max gate counts in brackets above columns
    tft.setCursor(158, 40);
    tft.printf("(%d)", radar.max_moving_gate);
    tft.setCursor(208, 40);
    tft.printf("(%d)", radar.max_stationary_gate);

    // Line 2 of header: Main labels
    tft.setCursor(5, 50);
    tft.print("Gate  Range             Moving Static");
    tft.drawFastHLine(5, 62, 230, TFT_DARKGRAY);

    Serial.println("\n--- HLK-LD2410C Current Gate Settings ---");
    Serial.println("=========================================");
    Serial.println(" GATE | MOVING SENSITIVITY | STATIC SENSITIVITY");
    Serial.println("-----------------------------------------");

    // 3. Render Table Rows (Shifted slightly down by 8 pixels to make room for 2-line header)
    for (uint8_t currentGate = 0; currentGate <= 8; currentGate++) {
      int yRowPosition = 66 + (currentGate * 14);

      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(10, yRowPosition);
      tft.printf("%d", currentGate);

      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setCursor(40, yRowPosition);
      float startDistance = currentGate * 0.75;
      float endDistance = (currentGate + 1) * 0.75;
      tft.printf("%.2fm-%.2fm", startDistance, endDistance);

      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setCursor(152, yRowPosition);
      tft.printf("%3d%%", radar.motion_sensitivity[currentGate]);

      tft.setTextColor(TFT_PINK, TFT_BLACK);
      tft.setCursor(202, yRowPosition);
      tft.printf("%3d%%", radar.stationary_sensitivity[currentGate]);

      // Serial diagnostics
      Serial.print("  ");
      Serial.print(currentGate);
      Serial.print("   |         ");
      Serial.print(radar.motion_sensitivity[currentGate]);
      Serial.print("%");
      if (radar.motion_sensitivity[currentGate] < 10) Serial.print("  ");
      else if (radar.motion_sensitivity[currentGate] < 100) Serial.print(" ");
      Serial.print("        |         ");
      Serial.print(radar.stationary_sensitivity[currentGate]);
      Serial.println("%");
    }

    // 4. Footer Section (Shifted down by 8 pixels accordingly)
    tft.drawFastHLine(5, 194, 230, TFT_DARKGRAY);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(5, 202);
    tft.printf("Timeout Delay: %d seconds", radar.sensor_idle_time);
    tft.drawFastHLine(0, 218, 240, TFT_DARKGRAY);

    Serial.println("=========================================");
    Serial.print("Max Moving Gate:    ");
    Serial.println(radar.max_moving_gate);
    Serial.print("Max Static Gate:    ");
    Serial.println(radar.max_stationary_gate);
    Serial.print("Timeout Delay:      ");
    Serial.println(radar.sensor_idle_time);
    Serial.println("=========================================\n");

  } else {
    // If request fails, print simple error message
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(5, 60);
    tft.print("Failed to load Gate Config!");
    Serial.println("FAILED to retrieve configuration from sensor.");
  }
}

// Robust function to handle rolling through 5 modes via short clicks and triggering actions via long click
void checkHardwareButton() {
  // Check if button is pressed (LOW)
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    delay(50);                                         // Hard debounce delay to ensure solid contact
    if (digitalRead(BOOT_BUTTON_PIN) == HIGH) return;  // False trigger shield

    unsigned long pressStartTime = millis();
    bool isLongClick = false;

    Serial.print("Button hold started in mode: ");
    Serial.println(currentDisplayMode);

    // Measure how long the button is being held down
    while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      // Check for 3 seconds hold time
      if (millis() - pressStartTime >= 3000) {
        isLongClick = true;
        Serial.println(">>> 3 SECONDS TARGET REACHED! <<<");
        break;  // Exit loop early while button is still held
      }
      delay(10);
    }

    // If it was a long click, wait until the user actually releases the button
    if (isLongClick) {
      Serial.println("Waiting for button release after long click...");
      while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        delay(10);
      }
      delay(200);  // Clear trailing physical switch contact bounces

      // --- EXECUTE LONG CLICK ACTIONS ---
      if (currentDisplayMode == DISPLAY_CALIBRATION) {
        Serial.println("Executing: runAutoCalibration()");
        runAutoCalibration();
      } else if (currentDisplayMode == DISPLAY_LOAD_CALIBRATION) {
        Serial.println("Executing: runLoadStoredThresholds()");
        runLoadStoredThresholds();
      } else if (currentDisplayMode == DISPLAY_SET_BASIC) {
        Serial.println("Executing: runSetBasicConfig()");
        runSetBasicConfig();
      } else if (currentDisplayMode == DISPLAY_RESET) {
        Serial.println("Executing: runFactoryReset()");
        runFactoryReset();
      }
    } else {
      // --- EXECUTE SHORT CLICK ACTIONS (ROLLING MENUS) ---
      Serial.println("Executing short click mode shift...");

      if (currentDisplayMode == DISPLAY_STANDARD) {
        currentDisplayMode = DISPLAY_REALTIME;
        tft.fillScreen(TFT_BLACK);
        radar.requestStartEngineeringMode();
        delay(100);
        Serial.println("Switched to: REALTIME EQUALIZER MODE");

      } else if (currentDisplayMode == DISPLAY_REALTIME) {
        currentDisplayMode = DISPLAY_CALIBRATION;
        radar.requestEndEngineeringMode();
        tft.fillScreen(TFT_BLACK);
        Serial.println("Switched to: CALIBRATION PREVIEW MODE");

      } else if (currentDisplayMode == DISPLAY_CALIBRATION) {
        currentDisplayMode = DISPLAY_LOAD_CALIBRATION;
        tft.fillScreen(TFT_BLACK);

        // Render Load Calibration UI
        tft.setTextSize(2);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.drawString("LOAD CALIBRATION", 240 / 2, 60);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Hold BOOT 3s", 240 / 2, 130);
        tft.drawString("to upload arrays", 240 / 2, 160);
        tft.setTextColor(TFT_DARKGRAY, TFT_BLACK);
        tft.drawString("Short click to skip", 240 / 2, 240);
        tft.setTextDatum(TL_DATUM);
        Serial.println("Switched to: LOAD CALIBRATION PREVIEW MODE");
      } else if (currentDisplayMode == DISPLAY_LOAD_CALIBRATION) {
        currentDisplayMode = DISPLAY_SET_BASIC;
        drawSetBasicPreview();  // Draws the 3x3 table immediately
        Serial.println("Switched to: SET BASIC CONFIG PREVIEW MODE");
      } else if (currentDisplayMode == DISPLAY_SET_BASIC) {
        currentDisplayMode = DISPLAY_RESET;
        tft.fillScreen(TFT_BLACK);

        // Render Factory Reset UI
        tft.setTextSize(2);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("FACTORY RESET", 240 / 2, 60);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Hold BOOT 3s", 240 / 2, 130);
        tft.drawString("for Factory Reset", 240 / 2, 160);
        tft.setTextColor(TFT_DARKGRAY, TFT_BLACK);
        tft.drawString("Short click to skip", 240 / 2, 240);
        tft.setTextDatum(TL_DATUM);
        Serial.println("Switched to: FACTORY RESET PREVIEW MODE");
      } else if (currentDisplayMode == DISPLAY_RESET) {
        currentDisplayMode = DISPLAY_STANDARD;
        drawStaticUserInterface();
        Serial.println("Switched to: STANDARD MODE");
      }

      // Wait for release of the short click
      while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        delay(10);
      }
      delay(150);  // Anti-bounce shield
    }
  }
}

// Renders the 3x3 table preview screen in large TextSize 2
void drawSetBasicPreview() {
    tft.fillScreen(TFT_BLACK);
    
    // Header
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("Set Basic Config", 240 / 2, 10);
    tft.setTextDatum(TL_DATUM);
    
    tft.drawFastHLine(0, 34, 240, TFT_DARKGRAY);

    // Fetch current values from the sensor
    if (radar.requestCurrentConfiguration()) {
        delay(100);

        // Column Headers in TextSize 2
        tft.setTextSize(2);
        tft.setTextColor(TFT_GOLD, TFT_BLACK);
        tft.setCursor(0, 44);   tft.print("Param");
        tft.setCursor(132, 44); tft.print("Act");
        tft.setCursor(186, 44); tft.print("New");
        tft.drawFastHLine(0, 64, 240, TFT_DARKGRAY);

        // Row 1: Timeout Delay
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(0, 76);   tft.print("Timeout");
        tft.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
        tft.setCursor(132, 76); tft.printf("%ds", radar.sensor_idle_time);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(186, 76); tft.printf("%ds", TARGET_TIMEOUT_SEC);

        // Row 2: Max Moving Gates
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(0, 106);   tft.print("Mov Gates");
        tft.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
        tft.setCursor(132, 106); tft.printf("%d", radar.max_moving_gate);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(186, 106); tft.printf("%d", TARGET_MAX_MOV_GATES);

        // Row 3: Max Stationary Gates
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(0, 136);   tft.print("Stat Gates");
        tft.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
        tft.setCursor(132, 136); tft.printf("%d", radar.max_stationary_gate);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(186, 136); tft.printf("%d", TARGET_MAX_STAT_GATES);

        tft.drawFastHLine(0, 164, 240, TFT_DARKGRAY);
    } else {
        tft.setTextSize(2);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(10, 85);
        tft.print("Read Config Error!");
    }

    // Long press instructions at the bottom (TextSize 2)
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Hold BOOT 3s", 240 / 2, 185);
    tft.drawString("to save values", 240 / 2, 210);

    tft.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    tft.drawString("Short click to skip", 240 / 2, 275);
    tft.setTextDatum(TL_DATUM);
}

// Helper function to draw centered status messages in Portrait Mode (240x320)
void drawCenteredMessages(uint16_t bgColor, uint16_t textColor, const char* line1, const char* line2 = "", int countdown = -1) {
  tft.fillScreen(bgColor);
  tft.setTextColor(textColor, bgColor);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);  // Top-Center alignment

  // Line 1 centered at Y = 80
  tft.drawString(line1, 240 / 2, 80);

  // Line 2 centered at Y = 120
  if (strlen(line2) > 0) {
    tft.drawString(line2, 240 / 2, 120);
  }

  // Big countdown number centered at Y = 180 (Text Size 5)
  if (countdown >= 0) {
    tft.setTextSize(5);
    char numStr[4];
    sprintf(numStr, "%d", countdown);
    tft.drawString(numStr, 240 / 2, 180);
  }

  tft.setTextDatum(TL_DATUM);  // Reset alignment to default
}

// Writes basic configuration using the global target constants
void runSetBasicConfig() {
    radar.requestEndEngineeringMode();
    delay(200);

    drawCenteredMessages(TFT_BLUE, TFT_WHITE, "SAVING BASIC...", "WRITING TO RADAR");

    // Parameters: (max_moving_gate, max_stationary_gate, inactivity_timer_seconds)
    if (radar.setMaxValues(TARGET_MAX_MOV_GATES, TARGET_MAX_STAT_GATES, TARGET_TIMEOUT_SEC)) {
        delay(200);

        // Mandatory reboot to commit changes permanently to flash
        drawCenteredMessages(TFT_BLUE, TFT_WHITE, "SAVING TO FLASH", "REBOOTING SENSOR");
        radar.requestRestart();
        delay(1200);

        // Refresh local threshold arrays to keep RAM in sync
        if (radar.requestCurrentConfiguration()) {
            for (uint8_t gate = 0; gate <= 8; gate++) {
                globalMoveThresholds[gate] = radar.motion_sensitivity[gate];
                globalStatThresholds[gate] = radar.stationary_sensitivity[gate];
            }
        }

        // Dynamic success message using the configured constants
        char msgStr[24];
        sprintf(msgStr, "%d/%d GATES | %ds", TARGET_MAX_MOV_GATES, TARGET_MAX_STAT_GATES, TARGET_TIMEOUT_SEC);
        drawCenteredMessages(TFT_GREEN, TFT_BLACK, "BASIC SAVED!", msgStr);
        delay(2500);
    } else {
        drawCenteredMessages(TFT_RED, TFT_WHITE, "SAVE FAILED!", "COMMAND TIMEOUT");
        delay(2500);
    }

    // Return to standard home screen
    tft.fillScreen(TFT_BLACK);
    currentDisplayMode = DISPLAY_STANDARD;
    drawStaticUserInterface();
}

void setup() {
  Serial.begin(MONITOR_SERIAL_SPEED);
  delay(500);
  Serial.println(PROGRAM_VERSION);

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RADAR_OUT_PIN, INPUT);

  bool displayInitialized = initDisplay();
  setOrientation(DISPLAY_ORIENTATION_TOP);
  if (displayInitialized) {
    Serial.println("Display is initialized.");
  } else {
    Serial.println("ERROR: display initialization failed");
  }

  Serial1.begin(RADAR_SERIAL_SPEED, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  Serial1.setRxBufferSize(2048);  // increase buffer as Engineering mode gives a lot of data

  Serial.print("Connecting to radar sensor...");
  if (radar.begin(Serial1)) {
    Serial.println(" OK!");
    radar.requestFirmwareVersion();
    delay(100);
    radar.read();
  } else {
    Serial.println(" FAILED! Please check wiring.");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.println("Radar Conn Failed!");
    while (1)
      ;
  }

  fetchRadarThresholds();

  drawStaticUserInterface();
  liveDataSprite.createSprite(240, 105);
}

void runAutoCalibration() {
  // 1. START Engineering Mode so the sensor actually streams gate energies!
  tft.fillScreen(TFT_BLACK);
  radar.requestStartEngineeringMode();
  delay(200);

  // 2. Wait for confirmation button press
  drawCenteredMessages(TFT_BLACK, TFT_WHITE, "PRESS BOOT BTN", "TO START COUNTDOWN");
  while (digitalRead(BOOT_BUTTON_PIN) == HIGH) { delay(20); }
  delay(300);

  // 3. 10-Second-Countdown
  for (int sec = 10; sec > 0; sec--) {
    drawCenteredMessages(TFT_ORANGE, TFT_BLACK, "LEAVE THE ROOM", "CALIBRATION IN:", sec);
    delay(1000);
  }

  // 4. Measuring Phase (Sensor is in Engineering Mode and delivers real values now)
  drawCenteredMessages(TFT_RED, TFT_WHITE, "CALIBRATING...", "DO NOT ENTER ROOM");

  uint8_t maxMoveNoise[9] = { 0 };
  uint8_t maxStatNoise[9] = { 0 };
  const uint32_t MEASURE_DURATION_MS = 15000;
  uint32_t startMeasure = millis();

  while (Serial1.available()) { Serial1.read(); }

  while (millis() - startMeasure < MEASURE_DURATION_MS) {
    if (radar.read()) {
      for (uint8_t gate = 0; gate <= 8; gate++) {
        // Fetching the actual live streamed energy values
        uint8_t currentMoveEnergy = radar.movingEnergyAtGate(gate);
        uint8_t currentStatEnergy = radar.stationaryEnergyAtGate(gate);

        if (currentMoveEnergy > maxMoveNoise[gate]) { maxMoveNoise[gate] = currentMoveEnergy; }
        if (currentStatEnergy > maxStatNoise[gate]) { maxStatNoise[gate] = currentStatEnergy; }
      }
    }
    yield();
  }

  // 5. NOW close Engineering Mode to safely open the Configuration transmission channel
  radar.requestEndEngineeringMode();
  delay(200);

  drawCenteredMessages(TFT_BLUE, TFT_WHITE, "PROCESSING...", "SAVING TO RADAR");

  if (radar.requestCurrentConfiguration()) {
    delay(100);
    for (uint8_t gate = 0; gate <= 8; gate++) {
      int newMoveThresh = maxMoveNoise[gate] + 20;
      int newStatThresh = maxStatNoise[gate] + 15;

      if (newMoveThresh > 100) newMoveThresh = 100;
      if (newMoveThresh < 10) newMoveThresh = 10;
      if (newStatThresh > 100) newStatThresh = 100;
      if (newStatThresh < 10) newStatThresh = 10;

      radar.setGateSensitivityThreshold(gate, newMoveThresh, newStatThresh);
      delay(50);
    }

    // 6. Write to Flash and reboot
    drawCenteredMessages(TFT_BLUE, TFT_WHITE, "SAVING TO FLASH", "REBOOTING SENSOR");
    radar.requestRestart();
    delay(1200);

    // Re-sync local global arrays
    for (uint8_t gate = 0; gate <= 8; gate++) {
      globalMoveThresholds[gate] = radar.motion_sensitivity[gate];
      globalStatThresholds[gate] = radar.stationary_sensitivity[gate];
    }

    drawCenteredMessages(TFT_GREEN, TFT_BLACK, "CALIBRATION DONE!", "VALUES SAVED");
    delay(2500);
  } else {
    drawCenteredMessages(TFT_RED, TFT_WHITE, "SAVE FAILED!", "CONFIG TIMEOUT");
    delay(2500);
  }

  tft.fillScreen(TFT_BLACK);
  currentDisplayMode = DISPLAY_STANDARD;
  drawStaticUserInterface();
}

// Sequence to flash hardcoded global array data directly into the sensor registers
void runLoadStoredThresholds() {
  // 1. Ensure Engineering Mode is inactive so configuration commands are accepted
  radar.requestEndEngineeringMode();
  delay(200);

  drawCenteredMessages(TFT_MAGENTA, TFT_WHITE, "UPLOADING DATA...", "WRITING GATES 0-8");

  // 2. Open transmission tunnel configuration
  if (radar.requestCurrentConfiguration()) {
    delay(100);

    // Loop through all 9 Gates to apply your hardcoded custom threshold arrays
    for (uint8_t gate = 0; gate <= 8; gate++) {
      uint8_t moveThresh = calibrationMoveThresholds[gate];
      uint8_t statThresh = calibrationStatThresholds[gate];

      // Safety boundaries clamps (10% to 100%)
      if (moveThresh > 100) moveThresh = 100;
      if (moveThresh < 10) moveThresh = 10;
      if (statThresh > 100) statThresh = 100;
      if (statThresh < 10) statThresh = 10;

      radar.setGateSensitivityThreshold(gate, moveThresh, statThresh);
      delay(50);  // Pacing delay between serial bursts
    }

    // 3. MANDATORY REBOOT to flash parameters permanently
    drawCenteredMessages(TFT_BLUE, TFT_WHITE, "SAVING TO FLASH", "REBOOTING SENSOR");
    radar.requestRestart();
    delay(1200);  // Give the FMCW radar core power cycle time

    // 4. Force synchronization of graphic lines local arrays
    for (uint8_t gate = 0; gate <= 8; gate++) {
      globalMoveThresholds[gate] = radar.motion_sensitivity[gate];
      globalStatThresholds[gate] = radar.stationary_sensitivity[gate];
    }

    // Green Success screen
    drawCenteredMessages(TFT_GREEN, TFT_BLACK, "UPLOAD SUCCESSFUL!", "VALUES ACTIVE");
    delay(2500);
  } else {
    // Red Error screen
    drawCenteredMessages(TFT_RED, TFT_WHITE, "UPLOAD FAILED!", "CONFIG TIMEOUT");
    delay(2500);
  }

  // Clean up interface, return back to home screen operation
  tft.fillScreen(TFT_BLACK);
  currentDisplayMode = DISPLAY_STANDARD;
  drawStaticUserInterface();
}

// Full Factory Reset sequence tailored for the HLK-LD2410C
void runFactoryReset() {
  // 1. Ensure Engineering Mode is closed to accept system commands
  radar.requestEndEngineeringMode();
  delay(200);

  drawCenteredMessages(TFT_RED, TFT_WHITE, "RESETTING RADAR...", "COMMITTING TO FLASH");

  // 2. Execute factory reset via library command
  if (radar.requestFactoryReset()) {
    delay(500);

    // 3. Reboot the sensor to apply default parameters
    drawCenteredMessages(TFT_BLUE, TFT_WHITE, "REBOOTING SENSOR", "PLEASE WAIT");
    radar.requestRestart();
    delay(1200);

    // 4. Force a re-fetch of the default factory thresholds to clear global arrays
    for (uint8_t gate = 0; gate <= 8; gate++) {
      globalMoveThresholds[gate] = radar.motion_sensitivity[gate];
      globalStatThresholds[gate] = radar.stationary_sensitivity[gate];
    }

    // Green Success screen
    drawCenteredMessages(TFT_GREEN, TFT_BLACK, "RESET SUCCESSFUL!", "FACTORY DEFAULTS");
    delay(2500);
  } else {
    // Red Error screen
    drawCenteredMessages(TFT_RED, TFT_WHITE, "RESET FAILED!", "COMMAND TIMEOUT");
    delay(2500);
  }

  // Clean up interface, return back to home screen
  tft.fillScreen(TFT_BLACK);
  currentDisplayMode = DISPLAY_STANDARD;
  drawStaticUserInterface();
}

void handleStandardMode() {
  liveDataSprite.fillSprite(TFT_BLACK);
  Serial.println("=========================================");

  // Read the hardware OUT pin state
  bool isOutPinHigh = (digitalRead(RADAR_OUT_PIN) == HIGH);

  // 1. Line 1 (Y = 4): UART Presence Status
  liveDataSprite.setTextSize(2);
  liveDataSprite.setCursor(10, 4);
  if (radar.presenceDetected()) {
    liveDataSprite.setTextColor(TFT_GREEN);
    liveDataSprite.print("[ TARGET DETECTED ]");
    Serial.println("Status:   [ TARGET DETECTED ]");
  } else {
    liveDataSprite.setTextColor(TFT_RED);
    liveDataSprite.print("[ NO TARGET ]");
    Serial.println("Status:   [ No target present ]");
  }

  // 2. Line 2 (Y = 28): Hardware OUT Pin Status
  liveDataSprite.setCursor(10, 28);
  if (isOutPinHigh) {
    liveDataSprite.setTextColor(TFT_GREEN);
    liveDataSprite.print("OUT: DETECTED HIGH");
    Serial.println("OUT Pin:  DETECTED (HIGH)");
  } else {
    liveDataSprite.setTextColor(TFT_RED);
    liveDataSprite.print("OUT: EMPTY LOW");
    Serial.println("OUT Pin:  EMPTY (LOW)");
  }

  // 3. Line 3 & 4 (Y = 54 & Y = 78): Moving and Stationary Details
  if (radar.presenceDetected()) {
    liveDataSprite.setCursor(10, 54);
    if (radar.movingTargetDetected()) {
      liveDataSprite.setTextColor(TFT_ORANGE);
      liveDataSprite.printf("MOV:%3dcm E:%2d", radar.movingTargetDistance(), radar.movingTargetEnergy());
      Serial.print("Moving:   Distance: ");
      Serial.print(radar.movingTargetDistance());
      Serial.print(" cm | Energy: ");
      Serial.println(radar.movingTargetEnergy());
    } else {
      liveDataSprite.setTextColor(TFT_DARKGRAY);
      liveDataSprite.print("MOV: No Motion");
    }

    liveDataSprite.setCursor(10, 78);
    if (radar.stationaryTargetDetected()) {
      liveDataSprite.setTextColor(TFT_PINK);
      liveDataSprite.printf("STA:%3dcm E:%2d", radar.stationaryTargetDistance(), radar.stationaryTargetEnergy());
      Serial.print("Static:   Distance: ");
      Serial.print(radar.stationaryTargetDistance());
      Serial.print(" cm | Energy: ");
      Serial.println(radar.stationaryTargetEnergy());
    } else {
      liveDataSprite.setTextColor(TFT_DARKGRAY);
      liveDataSprite.print("STA: No Static");
    }
  } else {
    liveDataSprite.setTextColor(TFT_DARKGRAY);
    liveDataSprite.setCursor(10, 54);
    liveDataSprite.print("MOV: ---cm E:--");
    liveDataSprite.setCursor(10, 78);
    liveDataSprite.print("STA: ---cm E:--");
  }

  liveDataSprite.pushSprite(0, 215);
}

void handleStandardMode_old() {
  liveDataSprite.fillSprite(TFT_BLACK);
  Serial.println("=========================================");

  if (radar.presenceDetected()) {
    liveDataSprite.setTextColor(TFT_GREEN);
    liveDataSprite.setTextSize(2);
    liveDataSprite.setCursor(10, 5);
    liveDataSprite.print("[ TARGET DETECTED ]");

    Serial.println("Status:   [ TARGET DETECTED ]");

    liveDataSprite.setTextSize(2);
    liveDataSprite.setCursor(10, 40);
    if (radar.movingTargetDetected()) {
      liveDataSprite.setTextColor(TFT_ORANGE);
      liveDataSprite.printf("MOV:%3dcm E:%2d", radar.movingTargetDistance(), radar.movingTargetEnergy());
      Serial.print("Moving:   Distance: ");
      Serial.print(radar.movingTargetDistance());
      Serial.print(" cm | Energy: ");
      Serial.println(radar.movingTargetEnergy());
    } else {
      liveDataSprite.setTextColor(TFT_DARKGRAY);
      liveDataSprite.print("MOV: No Motion");
    }
    liveDataSprite.setCursor(10, 75);
    if (radar.stationaryTargetDetected()) {
      liveDataSprite.setTextColor(TFT_PINK);
      liveDataSprite.printf("STA:%3dcm E:%2d", radar.stationaryTargetDistance(), radar.stationaryTargetEnergy());
      Serial.print("Static:   Distance: ");
      Serial.print(radar.stationaryTargetDistance());
      Serial.print(" cm | Energy: ");
      Serial.println(radar.stationaryTargetEnergy());
    } else {
      liveDataSprite.setTextColor(TFT_DARKGRAY);
      liveDataSprite.print("STA: No Static");
    }
  } else {
    liveDataSprite.setTextColor(TFT_RED);
    liveDataSprite.setTextSize(2);
    liveDataSprite.setCursor(10, 5);
    liveDataSprite.print("[ NO TARGET ]");
    Serial.println("Status:   [ No target present ]");
    liveDataSprite.setTextSize(2);
    liveDataSprite.setTextColor(TFT_DARKGRAY);
    liveDataSprite.setCursor(10, 40);
    liveDataSprite.print("MOV: ---cm E:--");
    liveDataSprite.setCursor(10, 75);
    liveDataSprite.print("STA: ---cm E:--");
  }
  liveDataSprite.pushSprite(0, 215);
}

void handleRealTimeMode() {
  tft.startWrite();

  // 1. Draw Top Status Banner
  if (radar.presenceDetected()) {
    tft.fillRect(0, 0, 240, 30, TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setTextSize(2);
    tft.setCursor(20, 7);
    tft.print("LIVE RADAR ACTIVE");
  } else {
    tft.fillRect(0, 0, 240, 30, TFT_RED);
    tft.setTextColor(TFT_BLACK, TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(45, 7);
    tft.print("ROOM IS EMPTY");
  }

  // Draw basic layout lines for the graphic grid
  int groundY = 280;         // The bottom baseline of our graph bars
  int maxGraphHeight = 220;  // Maximum bar height for 100% energy

  tft.drawFastHLine(5, groundY + 2, 230, TFT_DARKGRAY);

  // 2. Loop through all 8 Gates (0 to 7) to render the graph bars
  for (uint8_t gate = 0; gate <= 7; gate++) {
    // Calculate horizontal start position for each dual-bar package (Total 8 blocks)
    int blockX = 8 + (gate * 29);  // 29 pixels spacing per gate package

    // Get live energy datasets (values range from 0 to 100)
    uint8_t movingEnergy = radar.movingEnergyAtGate(gate);
    uint8_t staticEnergy = radar.stationaryEnergyAtGate(gate);

    // Map the globally stored threshold percentages (0-100) to pixel heights
    int moveThreshHeight = map(globalMoveThresholds[gate], 0, 100, 0, maxGraphHeight);
    int statThreshHeight = map(globalStatThresholds[gate], 0, 100, 0, maxGraphHeight);

    // Map energy values to pixel heights
    int movingBarHeight = map(movingEnergy, 0, 100, 0, maxGraphHeight);
    int staticBarHeight = map(staticEnergy, 0, 100, 0, maxGraphHeight);

    // --- 1. Determine Dynamic Colors for Moving Bar ---
    uint16_t currentMoveColor;
    if (movingBarHeight >= moveThreshHeight && moveThreshHeight > 0) {
      currentMoveColor = TFT_WHITE;  // ACTIVE: Threshold reached/exceeded -> White
    } else {
      currentMoveColor = TFT_DARKGRAY;  // PASSIVE: Below threshold -> Light/Dark Grey (TFT_DARKGRAY)
    }

    // --- 2. Determine Dynamic Colors for Stationary Bar ---
    uint16_t currentStatColor;
    if (staticBarHeight >= statThreshHeight && statThreshHeight > 0) {
      currentStatColor = TFT_RED;  // ACTIVE: Threshold reached/exceeded -> Red
    } else {
      currentStatColor = TFT_ORANGE;  // PASSIVE: Below threshold -> Orange
    }

    // --- 3. Render Moving Energy Bar Component ---
    // Draw active bar with dynamic color, and clear the space above it
    tft.fillRect(blockX, groundY - movingBarHeight, 11, movingBarHeight, currentMoveColor);
    tft.fillRect(blockX, groundY - maxGraphHeight, 11, maxGraphHeight - movingBarHeight, TFT_BLACK);

    // --- 4. Render Stationary Energy Bar Component ---
    tft.fillRect(blockX + 12, groundY - staticBarHeight, 11, staticBarHeight, currentStatColor);
    tft.fillRect(blockX + 12, groundY - maxGraphHeight, 12, maxGraphHeight - staticBarHeight, TFT_BLACK);

    // --- 5. Draw 2px Thick Threshold Lines (Green) ---
    // Overlays the bars perfectly for visual reference
    if (moveThreshHeight > 0) {
      tft.drawFastHLine(blockX, groundY - moveThreshHeight, 11, TFT_GREEN);
      tft.drawFastHLine(blockX, groundY - moveThreshHeight + 1, 11, TFT_GREEN);
    }

    if (statThreshHeight > 0) {
      tft.drawFastHLine(blockX + 12, groundY - statThreshHeight, 11, TFT_GREEN);
      tft.drawFastHLine(blockX + 12, groundY - statThreshHeight + 1, 11, TFT_GREEN);
    }

    // 3. Render Bottom Labels (Gate Numbers and Distance metrics)
    tft.setTextSize(1);
    tft.setCursor(blockX + 6, groundY + 8);
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.printf("G%d", gate);

    // Tiny meters stamp text under every bar package
    tft.setCursor(blockX, groundY + 22);
    tft.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    float metricDistance = (gate * 0.75) + 0.75;
    tft.printf("%.1fm", metricDistance);
  }

  tft.endWrite();
}

// Function to display instructions while staying inside the CALIBRATION mode state
void handleCalibrationMode() {
  static unsigned long lastCalUI = 0;

  // Refresh instructions screen at a relaxed 250ms cadence
  if (millis() - lastCalUI > 250) {
    lastCalUI = millis();

    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);  // Center alignment

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("CALIBRATION MODE", 240 / 2, 60);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Hold BOOT 3s", 240 / 2, 130);
    tft.drawString("to start routine", 240 / 2, 160);

    tft.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    tft.drawString("Short click to skip", 240 / 2, 240);

    tft.setTextDatum(TL_DATUM);  // Reset alignment
  }
}

// Function to display instructions while staying inside the LOAD CALIBRATION mode state
void handleLoadCalibrationMode() {
  static unsigned long lastLoadUI = 0;

  if (millis() - lastLoadUI > 250) {
    lastLoadUI = millis();

    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);

    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.drawString("LOAD CALIBRATION", 240 / 2, 60);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Hold BOOT 3s", 240 / 2, 130);
    tft.drawString("to upload arrays", 240 / 2, 160);

    tft.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    tft.drawString("Short click to skip", 240 / 2, 240);

    tft.setTextDatum(TL_DATUM);
  }
}


// Function to display instructions while staying inside the RESET mode state
void handleResetMode() {
  static unsigned long lastResetUI = 0;

  if (millis() - lastResetUI > 250) {
    lastResetUI = millis();

    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);  // Center alignment

    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("FACTORY RESET", 240 / 2, 60);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Hold BOOT 3s", 240 / 2, 130);
    tft.drawString("for Factory Reset", 240 / 2, 160);

    tft.setTextColor(TFT_DARKGRAY, TFT_BLACK);
    tft.drawString("Short click to skip", 240 / 2, 240);

    tft.setTextDatum(TL_DATUM);  // Reset alignment
  }
}

void loop() {
  radar.read();
  checkHardwareButton();

  if (radar.isConnected()) {
    static unsigned long lastOutputTimestamp = 0;
    if (millis() - lastOutputTimestamp > 100) {  // Fast 100ms update rate for fluid equalizer animation
      lastOutputTimestamp = millis();

      // Handle logic based on the current active enum state
      switch (currentDisplayMode) {
        case DISPLAY_STANDARD:
          // --- STANDARD MODE (Show stored thresholds or simple UI) ---
          handleStandardMode();
          // TODO: Place your previous layout code for zoomDisplayMode == false here
          break;

        case DISPLAY_REALTIME:
          // --- GIANT REAL-TIME EQUALIZER MODE ---
          handleRealTimeMode();
          // Your optimized "Realtime Gates" equalizer logic belongs here
          // updateTFTDisplay(); (or whatever your loop routine calls)
          break;

        case DISPLAY_CALIBRATION:
          // --- RADAR AUTOCALIBRATION MODE ---
          handleCalibrationMode();
          break;

        case DISPLAY_LOAD_CALIBRATION:
          // --- SHOW LOAD CALIBRATION PREVIEW (Waiting for Long Click) ---
          handleLoadCalibrationMode();
          break;

        case DISPLAY_SET_BASIC: break;

        case DISPLAY_RESET:
          // --- SHOW RESET PREVIEW ---
          handleResetMode();
          break;
      }
    }
  }
}