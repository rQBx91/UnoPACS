## UnoPACS: A Simple Physical Access Control System Using Arduino Uno 

UnoPACS is a simple physical access control system using an Arduino Uno board, MFRC522 RFID reader and DS1302 real time clock module.

### Features
* Use of master card for adding new cards
* Uses Built-In EEPROM for storing card ids and keeping logs
* Adjustable number of cards and log entries (dynamic memory range for total cards)

### Pin Configuration

| MFRC522 | Arduino Uno |
|---------|-------------|
| RST     | 9           |
| SDA(SS) | 10          |
| MOSI    | 11          |
| MISO    | 12          |
| SCK     | 13          |

| DS1302   | Arduino Uno |
|----------|-------------|
| CLK/SCLK | 5           |
| DAT/IO   | 4           |
| RST/CE   | 2           |
| VCC      | 3.3v - 5v   |
| GND      | GND         |