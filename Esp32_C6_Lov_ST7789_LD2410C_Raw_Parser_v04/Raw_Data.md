Raw Data of HLK LD2410C Sensor in Minimal Reporting

F4 F3 F2 F1 0D 00 02 AA 03 46 00 64 38 00 64 29 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 41 00 64 46 00 64 2A 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 45 00 64 41 00 64 2B 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 1E 00 64 45 00 64 27 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 4A 00 64 1E 00 64 29 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 2E 00 64 4A 00 64 27 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 1E 00 64 2E 00 64 23 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 62 00 64 1E 00 64 29 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 2D 00 64 62 00 64 27 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 32 00 64 2D 00 64 26 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 46 00 64 32 00 64 28 00 55 00 F8 F7 F6 F5  <- [PACKET END]
F4 F3 F2 F1 0D 00 02 AA 03 2B 00 64 46 00 64 26 00 55 00 F8 F7 F6 F5  <- [PACKET END]

Ja, das ist problemlos möglich! Die von dir bereitgestellten Daten entsprechen exakt dem Standard-Daten-Ausgabeformat des HLK-LD2410 Sensors (im Datenblatt oft auch fälschlicherweise als "Normal Mode" oder "Basic Reporting Mode" bezeichnet). Das Protokoll arbeitet im Little-Endian-Format (das niederwertigste Byte kommt zuerst), weshalb 2-Byte-Werte für die korrekte Umrechnung umgedreht werden müssen.
Hier ist die detaillierte Aufteilung und das exakte Parsing für ein einzelnes Datenpaket (exemplarisch anhand deines allerersten Datensatzes): [1] (https://naylampmechatronics.com/img/cms/001080/Protocolo_comunicacion_serial_LD2410C.pdf), [2] (https://docs.rs/ld2410)

Exakte Feld-Aufteilung für ein Paket

Rohdaten: F4 F3 F2 F1 0D 00 02 AA 03 46 00 64 38 00 64 29 00 55 00 F8 F7 F6 F5
• Field 01: Header
	• Roh-Bytes: F4 F3 F2 F1
	• Bedeutung: Start des Datensatzes (Frame Header). [1] (https://docs.rs/ld2410), [2] (https://naylampmechatronics.com/img/cms/001080/Protocolo_comunicacion_serial_LD2410C.pdf)
• Field 02: Intra-frame Data Length
	• Roh-Bytes: 0D 00
	• Bedeutung: Länge der nachfolgenden Daten bytes innerhalb des Frames bis vor den Footer (Little Endian für 0x000D = 13 Bytes).
• Field 03: Data Type
	• Roh-Bytes: 02
	• Bedeutung: Datentyp-Identifikator (0x02 = Normaler / Standard-Betriebsmodus). [1] (https://api-docs.esphome.io/ld2410_8cpp_source)
• Field 04: Head Check / Data Head
	• Roh-Bytes: AA
	• Bedeutung: Protokoll-spezifisches Start-Byte für Zielwerte. [1] (https://docs.rs/ld2410)
• Field 05: Target Status
	• Roh-Bytes: 03
	• Bedeutung: Aktueller Erkennungsstatus des Ziels.
	• Wert: 0x03 = Sowohl bewegtes als auch statisches Ziel erkannt (0x00 = Kein Ziel, 0x01 = Bewegtes Ziel, 0x02 = Statisches Ziel) [0x03 = Beides]. [1] (https://github.com/mgiesen/LD2410), [2] (https://api-docs.esphome.io/ld2410_8cpp_source)
• Field 06: Movement Target Distance
	• Roh-Bytes: 46 00
	• Bedeutung: Entfernung zum bewegten Objekt (Little Endian für 0x0046).
	• Wert: 70 Hex = 70 cm (0,70 Meter). [1] (https://github.com/mgiesen/LD2410)
• Field 07: Movement Target Energy
	• Roh-Bytes: 64
	• Bedeutung: Signalstärke/Energie des bewegten Objekts.
	• Wert: 64 Hex = 100 % (Wertebereich 0–100). [1] (https://github.com/mgiesen/LD2410)
• Field 08: Stationary Target Distance
	• Roh-Bytes: 38 00
	• Bedeutung: Entfernung zum statischen/ruhenden Objekt (Little Endian für 0x0038).
	• Wert: 38 Hex = 56 cm (0,56 Meter). [1] (https://github.com/mgiesen/LD2410)
• Field 09: Stationary Target Energy
	• Roh-Bytes: 64
	• Bedeutung: Signalstärke/Energie des statischen Objekts.
	• Wert: 64 Hex = 100 % (Wertebereich 0–100). [1] (https://github.com/mgiesen/LD2410)
• Field 10: Detection Distance
	• Roh-Bytes: 29 00
	• Bedeutung: Berechnete/unterstützte Erkennungsdistanz (Little Endian für 0x0029).
	• Wert: 29 Hex = 41 cm (0,41 Meter). [1] (https://github.com/mgiesen/LD2410)
• Field 11: Tail Check / Data Tail
	• Roh-Bytes: 55 00
	• Bedeutung: Protokoll-spezifisches End-Byte für die Zielwerte. [1] (https://docs.rs/ld2410)
• Field 12: Footer
	• Roh-Bytes: F8 F7 F6 F5
	• Bedeutung: Ende des Datensatzes (Frame Footer). [1] (https://docs.rs/ld2410)

Zusammenfassung deiner Pakete im Zeitverlauf

Wenn man deine restlichen Pakete nacheinander parst, sieht man sehr gut, wie sich die Werte verändern (der Status bleibt bei allen Paketen auf 0x03 = Bewegung + Statisch): [1] (https://api-docs.esphome.io/ld2410_8cpp_source)

Paket #	Bewegung Distanz	Bewegung Energie	Statisch Distanz	Statisch Energie
1 (46 00 / 38 00)	70 cm	100 %	56 cm	100 %
2 (41 00 / 46 00)	65 cm	100 %	70 cm	100 %
3 (45 00 / 41 00)	69 cm	100 %	65 cm	100 %
4 (1E 00 / 45 00)	30 cm	100 %	69 cm	100 %
5 (4A 00 / 1E 00)	74 cm	100 %	30 cm	100 %
6 (2E 00 / 4A 00)	46 cm	100 %	74 cm	100 %
7 (1E 00 / 2E 00)	30 cm	100 %	46 cm	100 %
8 (62 00 / 1E 00)	98 cm	100 %	30 cm	100 %

Fazit der Daten: Der Sensor registriert durchgehend maximale Signalenergien (100 %) ganz nah vor dem Sensor (alle Werte bewegen sich im Bereich zwischen 30 cm und knapp 1 Meter). Das ist ein typisches Verhalten, wenn man den Sensor testweise direkt vor sich auf dem Schreibtisch liegen hat und sich leicht bewegt oder atmet. [1] (https://github.com/mgiesen/LD2410), [2] (https://www.espboards.dev/sensors/ld2410/)

