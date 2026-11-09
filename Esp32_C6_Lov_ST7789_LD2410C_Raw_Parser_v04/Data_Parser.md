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
