Wohnzimmer:

ESP32-C6 PCB LovyanGFX ST7789 HLK LD2410C Human Presence Sensor Equalizer Lib V04
Display is initialized.
Connecting to radar sensor... OK!
Fetching current thresholds from LD2410...
Fetched Gate 0 -> Move Thresh: 37% | Stat Thresh: 15%
Fetched Gate 1 -> Move Thresh: 35% | Stat Thresh: 15%
Fetched Gate 2 -> Move Thresh: 35% | Stat Thresh: 76%
Fetched Gate 3 -> Move Thresh: 31% | Stat Thresh: 34%
Fetched Gate 4 -> Move Thresh: 27% | Stat Thresh: 27%
Fetched Gate 5 -> Move Thresh: 28% | Stat Thresh: 32%
Fetched Gate 6 -> Move Thresh: 27% | Stat Thresh: 32%
Fetched Gate 7 -> Move Thresh: 32% | Stat Thresh: 40%
Fetched Gate 8 -> Move Thresh: 30% | Stat Thresh: 80%
Thresholds successfully stored globally.

================ COPY FOR YOUR CODE ================
uint8_t calibrationMoveThresholds[9] = { 37, 35, 35, 31, 27, 28, 27, 32, 30 };
uint8_t calibrationStatThresholds[9] = { 15, 15, 76, 34, 27, 32, 32, 40, 80 };

Factory Data:

Fetching current thresholds from LD2410...
Fetched Gate 0 -> Move Thresh: 50% | Stat Thresh: 15%
Fetched Gate 1 -> Move Thresh: 50% | Stat Thresh: 15%
Fetched Gate 2 -> Move Thresh: 40% | Stat Thresh: 40%
Fetched Gate 3 -> Move Thresh: 30% | Stat Thresh: 40%
Fetched Gate 4 -> Move Thresh: 20% | Stat Thresh: 30%
Fetched Gate 5 -> Move Thresh: 15% | Stat Thresh: 30%
Fetched Gate 6 -> Move Thresh: 15% | Stat Thresh: 20%
Fetched Gate 7 -> Move Thresh: 15% | Stat Thresh: 20%
Fetched Gate 8 -> Move Thresh: 15% | Stat Thresh: 20%
Thresholds successfully stored globally.

================ COPY FOR YOUR CODE ================
uint8_t calibrationMoveThresholds[9] = { 50, 50, 40, 30, 20, 15, 15, 15, 15 };
uint8_t calibrationStatThresholds[9] = { 15, 15, 40, 40, 30, 30, 20, 20, 20 };