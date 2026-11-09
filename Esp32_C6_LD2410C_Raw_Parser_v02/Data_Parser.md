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
-------------------------------------------------

Der HLK-LD2410C nutzt ein FMCW-Radar (Frequency Modulated Continuous Wave) und teilt den Raum vor sich in sogenannte "Distance Gates" (Entfernungs-Zonen von je ca. 75 cm) auf. Um präzise zu steuern, wie das System auf Anwesenheit reagiert, trennt der Sensor intern strikt zwischen Bewegung und Stillstand.
Die drei Distanzwerte unterscheiden sich wie folgt:

1. Move Distance (Bewegungs-Distanz)

• Bedeutung: Die Entfernung zum Objekt, das aktuell eine dynamische, deutliche Bewegung ausführt (z. B. Gehen, Laufen, heftiges Winken).
• Sensor-Logik: Der Sensor filtert hier gezielt nach Frequenzänderungen, die durch schnelle Positionswechsel entstehen. Wenn du den Raum betrittst, schlägt dieser Wert zuerst an.

2. Stationary Distance (Statische Distanz)

• Bedeutung: Die Entfernung zum Objekt, das sich kaum oder nur minimal bewegt (z. B. eine sitzende Person, die tippt, liest oder nur atmet).
• Sensor-Logik: Hier misst das Radar winzige Mikrobewegungen (wie das Heben und Senken des Brustkorbs beim Atmen). Wenn du dich im Raum auf einen Stuhl setzt und ruhig verweilst, friert die Move Distance oft ein oder springt wild umher, während die Stationary Distance stabil und exakt auf deiner Sitzposition bleibt.

3. Detect Distance (Erkennungs-Distanz / Kombinierte Distanz)

• Bedeutung: Dies ist der vom Sensor final berechnete Hauptwert, welcher Typ von Ziel (oder welches Ziel) gerade die logische Priorität hat.
• Sensor-Logik:
	• Wenn der Status 0x01 ist (nur Bewegung), entspricht die Detect Distance der Move Distance.
	• Wenn der Status 0x02 ist (nur statisch), entspricht sie der Statistary Distance.
	• Wenn der Status 0x03 ist (so wie in deinen Beispieldaten, wo beide Zustände gleichzeitig aktiv sind), nutzt die Firmware des LD2410 einen internen Algorithmus. Meistens priorisiert sie das Ziel, das näher am Sensor liegt oder die höhere Signalenergie aufweist, um eine zuverlässige "Präsenzerkennung" zu gewährleisten.

Warum macht der Sensor das?

Diese Trennung ist der größte Vorteil gegenüber klassischen PIR-Bewegungsmeldern. Wenn du ein smartes Licht steuern willst, nutzt du die Move Distance zum schnellen Einschalten (Reaktionszeit) und die Detect Distance oder Stationary Distance, um zu verhindern, dass das Licht ausgeht, während du absolut still auf der Couch liegst oder liest.
Möchtest du wissen, wie man über die HLKRadarTool-App oder serielle Befehle die Empfindlichkeit (Gates) für diese beiden Typen getrennt voneinander einstellt, um beispielsweise Fehlalarme durch Ventilatoren oder Vorhänge zu verhindern?

Dass die Detect Distance (32 cm) deutlich kürzer ist als die gemessene Bewegung (73 cm) und der Stillstand (63 cm), liegt an einem Software- "Trick" der LD2410-Firmware, wenn der Sensor extrem nah am Limit betrieben wird.
Das Verhalten lässt sich durch zwei Faktoren erklären:

1. Das Blind-Gate (Zone 0) und Festzielunterdrückung

Der LD2410 teilt die Welt in Zonen (Gates) von je 75 cm ein.
• Gate 0 reicht von 0 bis 75 cm.
• In diesem Bereich (besonders unter 50 cm) kämpft das Radar mit extrem starken Eigenreflexionen (z. B. vom Gehäuse, der Platine oder der Tischplatte, auf der er liegt).
Wenn sowohl Bewegung als auch statische Energie bei 100 % liegen, ist der Sensor in diesem Nahbereich völlig übersteuert. Die interne Firmware versucht dann verzweifelt, ein "Hintergrundrauschen" herauszurechnen. Dabei kommt es bei den Entfernungsalgorithmen im ersten Gate (0–75 cm) oft zu mathematischen Rundungs- oder Kombinationsfehlern, die den zusammengesetzten Detect-Wert künstlich nach unten ziehen.

2. Radarquerschnitt und Phasenverschiebung

Da du dich sehr nah am Sensor befindest, reflektiert dein gesamter Körper (Hände, Brustkorb, Kopf) die Radarwellen.
• Die Hand bewegt sich vielleicht in 73 cm Entfernung.
• Der Oberkörper sitzt ruhig in 63 cm Entfernung.
• Ein Teil deines Arms oder das Kabel des ESP32 liegt aber möglicherweise nur 32 cm vom Sensor entfernt.
Für die Werte Move und Stat nutzt der Sensor getrennte Filter, die auf bestimmte Frequenzen ansprechen. Für die finale Detect Distance schaut der Sensor jedoch oft einfach stur nach dem absolut nächsten Peak, an dem überhaupt ein Signal reflektiert wird (auch wenn die Energie dafür intern anders gewichtet wurde). Er meldet dir mit den 32 cm also: "Hier fängt die Blockade an!"

Fazit

In der Praxis (wenn der Sensor an der Wand oder Decke montiert ist und Personen im normalen Abstand von 1 bis 5 Metern erfasst) laufen diese drei Werte sehr sauber synchron. Im extremen Nahbereich (< 75 cm) auf dem Schreibtisch neigt die Firmware des LD2410 zu genau diesem "Springen" des kombinierten Wertes.
Soll ich dir zeigen, wie du im Arduino-Code eine eigene Logik einbauen kannst, die im Nahbereich unter 75 cm den ungenauen Detect-Wert ignoriert und stattdessen den plausibleren Statistary- oder Move-Wert verwendet?
