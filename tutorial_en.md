# How to Use an HLK LD2410C Human Presence Sensor with an ESP32 (Beginner Tutorial)

In this tutorial, you will learn how to connect an LD2410C sensor to an ESP32, as well as how to read and evaluate the received data. All of this is possible without using an additional library.

I am currently planning an advanced-level tutorial for the LD2410C that covers how to process all data from the LD2410C sensor and allows for custom configuration. For this, I will be using the "ld2410" library by Nick Reynolds. I will update this post once the tutorial is live.

An important note at the beginning: throughout this tutorial, I will always refer to the ESP32, but this tutorial can be used with all processors in the ESP32 family. For example, I use an ESP32-S3 (LilyGo T-Display), an ESP32-C5, and an ESP32-C6 with an attached TFT display (ST7789 or ILI9341 chip). A quick tip: use the largest display possible, because for the distance tests, we will output the measured distance on the screen, and you should still be able to read the value from 6 meters away! :-)

## Topics of this Tutorial
* What is a Human Presence Sensor?
* How does a Human Presence Sensor work?
* Selecting the right sensor
* Technical specifications of the HLK LD2410C sensor
* Wiring the sensor to the ESP32
* Manufacturer support
* Initial contact with the sensor - Raw data without a library
* A simple example
* Notes on the practical test setup
* Excursus: How do I understand the received data?
* Final program
* Summary

---

## What is a Human Presence Sensor?
A Human Presence Sensor monitors a specific area in front of it and detects whether a person is located within this space. Additionally, a Human Presence Sensor detects whether the person in the room is moving or standing still/sitting. This makes these sensors significantly more powerful than traditional motion sensors ("PIR"), which can only detect actual movement.

A practical example is room lighting control: A PIR sensor detects a person entering the room and turns on the light. However, if the person sits quietly at a desk, the light will turn off again after a hold time. The Human Presence Sensor, on the other hand, behaves similarly but additionally detects the stationary person in the room and keeps the light turned on.

## How does a Human Presence Sensor work?
In contrast to a "PIR" ("Passive Infrared") sensor, which operates based on thermal imaging, a Human Presence Sensor is an active sensor that emits and receives radio waves (in this case, in the 24 GHz band), making it a "Radar Sensor." If people are present or moving in the room, the received signal changes, and the sensor reports presence.

### Why is the 24 GHz band used for this sensor?
This frequency band is reserved worldwide for "ISM" ("Industrial, Scientific, and Medical") applications and can be used license-free. The 24 GHz frequency band is particularly well-suited for motion detectors because the very short wavelengths precisely capture even the smallest movements while allowing for an extremely compact sensor design. Furthermore, the 24 GHz signal is insensitive to high temperatures (with a PIR sensor, a heater or a candle can trigger a false alarm).

Additionally, these waves easily penetrate (thin) materials such as plastic enclosures, glass, wood, or drywall panels, allowing the sensor to be installed completely out of sight.

Moreover, a special modulation of the radio signal is used called "FMCW" (Frequency-Modulated Continuous Wave), which enhances the sensor's performance. FMCW sensors do not just detect micro-movements (such as breathing); they can also divide the room into detection zones (e.g., "turn light on only when someone is sitting at the desk, but not when passing by in the hallway") and track multiple people simultaneously.

## Selecting the Right Sensor
The sensor used here, the LD2410C, is manufactured by the Chinese company Hilink Electronics Co., Ltd. (https://www.hlktech.com/en/), which distributes many other (similar) models alongside this one. If you want to buy an HLK Human Presence Sensor on sites like AliExpress or others, you must pay close attention to the exact model designation (here: LD2410C) because every single letter matters.

The following criteria are important when choosing a sensor:

### Room Size
The maximum room size is determined by the maximum detection distance and the detection angle. The manufacturer recommends ceiling mounting, for example; however, for convenience, I place my sensor on one side of the room. It does not matter whether the sensor is positioned vertically or horizontally.

### Power Supply
Generating a high-frequency signal is relatively power-intensive. Therefore, you must expect currents of up to 200 mA and provide a supply voltage of 5 Volts (some models can also manage with 3.3 Volts).

### Interface to the Microprocessor
The vast majority of sensors use the UART (Universal Asynchronous Receiver Transmitter) interface with 256000 baud or 115200 baud. The interface requires a two-wire connection ("TX" and "RX" channels). A few sensors additionally feature a Bluetooth Low Energy ("BLE") profile, which allows for remote parameter configuration and measurement queries.

### Sensor Form Factor
The electronic components are located on the sensor board, along with the radar transmitting and receiving antennas in the form of copper pads. The connection to the microprocessor (power and data) is often made using standard breadboard pins (Male) with a 2.54 mm pitch. However, there are variants like the LD2410B, which is technically identical to the LD2410C, but its breadboard pins have a 1.2 mm pitch. For the alternative option of soldering directly, you will need a super-fine soldering tip. Other sensors, like the LD2450, rely on a plug connection with a ZH 1.5 form factor.

### Availability of a Ready-to-Use Library
The sensors deliver a continuous data stream, and a library helps analyze and split the data, as well as modify the sensor settings. Therefore, check before buying whether an (Arduino) library is available for your intended sensor. Just like during the purchase, it is vital to meticulously read the technical description or the source code (again, every letter of the model name counts).

## Technical Specifications of the HLK LD2410C Sensor
For this tutorial, the choice fell on the HLK LD2410C sensor because, in my eyes, it is the most universal Human Presence Sensor available.

* **a) Room Size:** The maximum distance is 5 meters (according to the datasheet), whereby the space in front of the sensor is divided into 8 equal-sized zones or "Gates." The detection angle of the sensor is +/- 60 degrees, allowing it to monitor an area of around 26 square meters. The maximum width at the furthest point (5 meters) is about 8 meters.
* **b) Power Supply:** According to the datasheet, the sensor requires a stable 5 Volt power source and draws up to 200 mA of current. In my tests, however, the sensor also functioned flawlessly at 3.3 Volts.
* **c) Interface:** The LD2410C communicates with the ESP32 via a 2-wire UART interface at 256000 baud. The signal voltage level is 3.3 Volts, which is directly compatible with the ESP32 signal level. On the ESP32, I use the second UART interface so that I can still exchange external data with the processor via the default interface. Additionally, the sensor features a Bluetooth Low Energy (BLE) interface that can be switched on and off, which can be conveniently used via the manufacturer's own apps (Android and iOS). Of course, the ESP32 can also work with the sensor via BLE, enabling wireless communication (this is not part of this tutorial).
* **d) Form Factor:** The sensor has a size of 22x16 millimeters and possesses 5 soldered pins (male) at the standard breadboard spacing of 2.54 mm. The connections made to these 5 pins are: VCC (5 Volts), GND, OT1 (corresponds to the TX pin), RX, and OT2 (corresponds to the OUT pin). *Caution: The meaning of the OT1 and OT2 pins varies from model to model; please read the respective sensor documentation carefully!*
* **e) Libraries:** Since this sensor is the direct successor to the LD2410 sensor, several proven libraries exist for it. For this sensor (though not in this tutorial), I use the "ld2410" library by Nick Reynolds, which can be installed via the Arduino Library Manager or manually from https://github.com/ncmreynolds/ld2410 (current version is 0.2.2). The library provides access to all configuration and data fields of the LD2410C. *Caution: This library is not compatible with the LD2410S sensor!*

## Wiring the Sensor to the ESP32
In this tutorial, I am focusing solely on the Human Presence Sensor; you will need to handle the wiring of your own display.

If I use a display in an example program, it is controlled via the LovyanGFX library, as it works under esp32-boards 3.x and supports all of my ESP32 boards (ESP32, -S3, H2, C5, and C6). It can also be installed via the Library Manager (or manually from here: https://github.com/lovyan03/LovyanGFX), and I am using version 1.2.35. I worked with Arduino IDE version 2.3.10 and used esp32-boards version 3.3.11.

For this tutorial, you need 2 free GPIO pins for the UART interface. The pins between the sensor and the ESP32 board are connected as follows:

```
Sensor      - ESP32
VCC         - 5 Volts
GND         - GND
RX Pin      - TX Pin
OT1/TX Pin  - RX Pin
OT2 Pin     - Free GPIO (optional)
```

It is crucial that the TX and RX pins are cross-connected, meaning "RX to TX" and "TX to RX."

As previously written, the sensor's signal level is 3.3 Volts, so it can be connected directly to the ESP32's GPIOs.

I have also connected the OT2 pin (which is the OUT output) to the ESP32, as it provides digital "room is empty" or "room is occupied" information. When the sensor detects an occupancy, the signal level goes "HIGH"; otherwise, it remains "LOW." If you choose not to use this connection, the tutorial will still work perfectly.

## Manufacturer Support
The manufacturer provides the necessary information in a Google Drive archive on its website. As I discovered during my tests, the manufacturer's own information is unfortunately not always up to date, but it should still contain the correct data for our work.

You can find the Google Drive archive via this link: https://drive.google.com/drive/folders/1ypOlacBmmFXY6lDQ0f1hEJFmczNe-0WG. You should also take a look at the information for the LD2410B sensor, as this sensor is technically fully compatible with the LD2410C, and you can find additional information regarding the BLE interface there (Link: https://drive.google.com/drive/folders/16zI-fium_BZeP08EyQke0rWp0BJTMvw3).

In any case, you should download both PDFs: "HLK LD2410C Life Presence Sensor Module Data Sheet V1.00" and "HLK-LD2410C Serial communication protocol V1.07", as you will need them repeatedly when working with the sensor.

## Initial Contact with the Sensor - Raw Data Without a Library
I will start with the simplest example and make the received data progressively more readable:

### Sketch: ESP32 C6 LD2410 Raw V01
Our first sketch does not use a library but works exclusively with the raw data of the sensor in its factory default state.

This is the smallest possible program, which first initializes the two serial interfaces, listens on the second serial interface ("Serial1") inside the `loop()`, and displays the received data in hexadecimal encoding.

We receive a continuous stream of data that appears completely random at first glance:

```
ESP32-C6 HLK LD2410C Human Presence Sensor Raw V01
F4 F3 F2 F1 0D 00 02 AA 01 3B 00 64 00 00 00 00 00 55 00 F8 F7 F6 F5 F4 F3 F2 F1 0D 00 02 AA 01 31 00 64 00 00 00 00 00 55 00 F8 F7 F6 F5 F4 F3
```

### Sketch: ESP32 C6 LD2410 Raw Record Limiter V01
The basic structure corresponds to the "Raw" program; however, looking closely at the data reveals recurring patterns. We repeatedly see the bytes `F8 F7 F6 F5 F4 F3 F2 F1`. At this point, we look into the "Serial Communication Protocol" under item "2.3 Radar data output protocol" and find the crucial hints a bit later under "2.3.1 Reported data frame format." Table 9 reveals the fundamental structure of the sent data:

* **Frame header:** `F4 F3 F2 F1`
* **End of frame:** `F8 F7 F6 F5`

Thus, it is clear: every dataset ends with the "End of frame" data sequence. I have integrated a ring buffer into the Raw program, which is continuously queried and compared with the "End of frame" data sequence. Upon a match, the dataset is separated by a line break:

**Header:**
```cpp
// Buffer to detect the 4-byte footer: F8 F7 F6 F5
uint32_t footerCheckBuffer = 0;
```

**loop():**
```cpp
// Slide the incoming byte into our 32-bit footer history checker
footerCheckBuffer = (footerCheckBuffer << 8) | incomingByte;

// Check if the last 4 bytes match the LD2410 standard footer: F8 F7 F6 F5
if (footerCheckBuffer == 0xF8F7F6F5) {
  // Footer detected! Break line and mark packet boundary
  Serial.println(" <- [PACKET END]");
}
```

Here you can see the result:
```
F4 F3 F2 F1 0D 00 02 AA 03 46 00 64 38 00 64 29 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 41 00 64 46 00 64 2A 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 45 00 64 41 00 64 2B 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 1E 00 64 45 00 64 27 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 4A 00 64 1E 00 64 29 00 55 00 F8 F7 F6 F5  <- [PACKET END]
```

### Sketch: ESP32 C6 LD2410 Raw Parser V01
We look into the Serial Communication Protocol once more and split ("parse") the received data.

In section "2.3.2 Target data composition", all data fields contained within our dataset are listed.

In section 2.3.1, we already saw the basic structure: a "Frame Header", the length of the data in 2 bytes (fundamentally encoded in "Little Endian"), the actual data, and the "End of frame" sequence. We will now look at the actual data, which is broken down in 2.3.2. However, the order of the data is slightly confusing since it is shown in separate tables. Here is the exact sequence of the data and its meaning:

1. Data Type: Reporting mode
2. Head check: static data - Protocol Data Head (0xAA)
3. Target Status: what the sensor detected
4. Movement target distance (cm)
5. Move Energy
6. Stationary Distance (cm)
7. Stationary Energy
8. Detect Distance (cm)
9. Tail Check: static data - Protocol Data Tail (0x55 0x00)

When I send this data to the parser, I get the following output:

```
F4 F3 F2 F1 0D 00 02 AA 03 49 00 64 3F 00 64 20 00 55 00 F8 F7 F6 F5  <- [PACKET END]
------------------ PARSED DATA ------------------
Field 01: Header, 4 Bytes: F4 F3 F2 F1  | Meaning: Start of data
Field 02: Data Length, 2 Bytes: 0D 00  | Value: 13 Bytes within the frame
Field 03: Data Type, 1 Byte: 02  | Meaning: Normal Mode (0x02)
Field 04: Head Check, 1 Byte: AA  | Meaning: Protocol Data Head (0xAA)
Field 05: Target Status, 1 Byte: 03  | Value: 0x3 -> BOTH moving and stationary targets detected
Field 06: Move Distance, 2 Bytes: 49 00  | Value: 73 cm
Field 07: Move Energy, 1 Byte: 64  | Value: 100 %
Field 08: Stat Distance, 2 Bytes: 3F 00  | Value: 63 cm
Field 09: Stat Energy, 1 Byte: 64  | Value: 100 %
Field 10: Detect Distance, 2 Bytes: 20 00  | Value: 32 cm
Field 11: Tail Check, 2 Bytes: 55 00  | Meaning: Protocol Data Tail (0x55 0x00)
Field 12: Footer, 4 Bytes: F8 F7 F6 F5  | Meaning: End of Dataset
Status OUT Pin: Person detected in room.
-------------------------------------------------
```

**Result:** We have a fully usable parser because it gives us all the necessary information for downstream processing (turning a lamp on or off, etc.).

Please ensure that the displayed Data Type is "Normal Mode" (and not "Engineering Mode"), because only then can the simple parser split and output the dataset. If you have changed the mode due to any prior usage, you must reset it to "Normal Mode." I do not provide instructions for this in this tutorial, but the aforementioned "ld2410" library includes a corresponding function.

`Field 03: Data Type, 1 Byte: 02  | Meaning: Normal Mode (0x02)`

Crucial for everyone working with the raw data in the future: all multi-byte data fields are encoded in **Little Endian**, meaning the least significant byte is displayed first. Example: Move Distance is `0x49 0x00` = decimal 73 * 1 + decimal 0 * 256 = 73 cm.

If you connected the additional wire from OT1 to another GPIO, you can query the status of this pin using `digitalRead`. The sensor keeps the state of OT1 at LOW when it detects no person in the room. If a detection occurs, the value jumps to "HIGH", which equals "Detected."

Therefore, if you connect a small relay directly to the OUT pin, for example, you can trigger an action immediately without using a microcontroller at all.

## Notes on the Practical Test Setup
At this point, it is time to talk about the practical test setup in your programming lab. You will notice that the sensor constantly reports "detected", which is simply because the sensor is so sensitive that it always notices your presence "nearby." Even though the sensor's main direction of view faces away from the PCB, its waves propagate through the room and the receiver captures them.

For this reason, I chose an ESP32-C6 board with a large display that operates autonomously, meaning without a computer and Serial Monitor. The most important information is displayed large and in a color that makes it readable even from a distance of several meters.

After uploading a new version, I carry the setup into another room where a power bank provides the power supply, and I can do my tests in peace. Although the room is several meters long, I sometimes have to leave it entirely to trigger a "room is free" state on the sensor.

**Important Note:** Even when the last person leaves the room, the sensor does not immediately display "EMPTY" because it features a built-in timer ("No one duration"). Only after no one is left in the room is it triggered, and it switches to the LOW state (which means "EMPTY") after 5 seconds.

## Excursus: How Do I Understand the Received Data?
In the following, I will use a few terms that I will explain to you here:

* **Gate:** The area in front of the sensor is divided into 9 zones, each 75 cm wide. This results in a maximum distance of 6.75 meters in which the sensor performs a measurement. Each of these zones is called a "Gate." The numbering starts at 0, marking the area immediately in front of the sensor.
* **Energy:** If a human is present within a gate, the sensor perceives this as an energy field. The higher the value, the more "human" was detected. The values range between 0 and a maximum of 100.
* **Sensitivity Thresholds:** For each gate, there is a (modifiable) default setting determining at which Energy value it should output a "Detect." Crucially, lower values mean higher sensitivity. Example: The sensor measures an energy of "50" in a gate. If the threshold was set to "70", no "Detect" message is issued. If, however, the sensitivity threshold is set to "30", the Detect message is triggered. *Note: Different sensitivities can be set for motion and stationary detection.*
* **Stationary vs. Moving:** The sensor is capable of measuring the energy of a stationary or moving person separately for each individual gate. This is useful if you want to trigger different actions for both states. Example: A person walks past in Gate 8, and a person is sitting in Gate 2. The sensor can recognize these two distinct patterns and react accordingly.
* **Detect Hold Time (also "no one duration"):** This variable regulates how many seconds the sensor should hold the Out pin HIGH and display a "Detect" in the Serial Protocol after the last detection event.

## Final Program
As written above, practical testing can sometimes be a bit difficult because to read the Serial Monitor, you must sit at the computer, and since the LD2410C is nearby, you hardly ever reach the "Empty" state there. Therefore, I have extended the simple parser with an output function for a TFT display in 240x320 pixel portrait mode.

**Sketch Name:** `ESP32_C6_Lov_ST7789_LD2410_Raw_Parser_V04`

The program uses the LovyanGFX graphics library, which handles almost any TFT display on the ESP32. The parsed dataset continues to be output unmodified in the Serial Monitor, and the exact same data is also shown on the TFT display.

## Summary
The HLK LD2410C sensor is a fantastic addition for any developer of microprocessor applications. It can be used quickly and easily with an ESP32 and offers high quality regarding the measured data.

### App Source Code
You can find all the code collected in my GitHub repository. I added the Data Sheet and the Serial Communication Protocol as well, just in case the original source is not available.

**Happy coding!**
