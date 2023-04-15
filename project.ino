/**
 * MFRC522
 * --------------------------------------
 *             MFRC522      Arduino      
 *             Reader/PCD   Uno/101       
 * Signal      Pin          Pin           
 * --------------------------------------
 * RST/Reset   RST          9             
 * SPI SS      SDA(SS)      10            
 * SPI MOSI    MOSI         11 / ICSP-4  
 * SPI MISO    MISO         12 / ICSP-1   
 * SPI SCK     SCK          13 / ICSP-3   
 *
 * RTCDS1302
 * DS1302 CLK/SCLK --> 5
 * DS1302 DAT/IO --> 4
 * DS1302 RST/CE --> 2
 * DS1302 VCC --> 3.3v - 5v
 * DS1302 GND --> GND 
 *   
 * EEPROM management:
 * |0					    |TOTOAL_CARDS * 4 + 2   	1023|
 * |--------------------------------------------------------|
 * |tail,id1,id2,...........|tail,(i,date),(i,date).........|
 * |--------------------------------------------------------|
 * 
 * Note: i in (i,date) is index of the associated id
 * 
 * tail --> short (2 bytes)
 * id1, id2, .. --> uint32_t (4 bytes)
 * (i,date) --> byte, uint32_t (5 bytes)
 * 
**/


#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

ThreeWire myWire(4,5,2); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above
MFRC522 rfid(SS_PIN, RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define TOTOAL_CARDS 100
#define LOG_START ((TOTOAL_CARDS * 4) + 2)
#define LED_PIN 7
uint32_t master = 3931722563; // Master card id

// Begin setup
void setup() {
	pinMode(LED_PIN, OUTPUT); 
	Rtc.Begin();
	RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

	if (!Rtc.IsDateTimeValid()) 
	{
		// Common Causes:
		//    1) first time you ran and the device wasn't running yet
		//    2) the battery on the device is low or even missing

		Serial.println("RTC lost confidence in the DateTime!");
		Rtc.SetDateTime(compiled);
	}

		if (Rtc.GetIsWriteProtected())
	{
		Serial.println("RTC was write protected, enabling writing now");
		Rtc.SetIsWriteProtected(false);
	}

	if (!Rtc.GetIsRunning())
	{
		Serial.println("RTC was not actively running, starting now");
		Rtc.SetIsRunning(true);
	}

		RtcDateTime now = Rtc.GetDateTime();
	if (now < compiled) 
	{
		Serial.println("RTC is older than compile time!  (Updating DateTime)");
		Rtc.SetDateTime(compiled);
	}
	else if (now > compiled) 
	{
		    ("RTC is newer than compile time. (this is expected)");
	}
	else if (now == compiled) 
	{
		Serial.println("RTC is the same as compile time! (not expected but all is fine)");
	}
		
	Serial.begin(9600);
	while (!Serial);
	SPI.begin();
	rfid.PCD_Init();

	// Prepare the key (used both as key A and as key B)
	// using FFFFFFFFFFFFh which is the default at chip delivery from the factory
	for (byte i = 0; i < 6; i++) {
		key.keyByte[i] = 0xFF;
	}
}
// End setup

// Begin loop
void loop() {
  String command;
	if (Serial.available()){
		command = Serial.readString();
		command.trim();
		if (command == "Log"){
			showLog();      
		}
    if (command == "Id"){
			showId();      
		}
		if (command == "ResetAll"){
			resetAll();
		}
		if (command == "ResetId"){
			resetId();
		}
		if (command == "ResetLog"){
			resetLog();
		}
		if (command == "Time"){
		RtcDateTime time = Rtc.GetDateTime();
		printDateTime(time);
		Serial.println();                
		}
	}
  
	RtcDateTime now = Rtc.GetDateTime();

	if (!now.IsValid())
	{
		Serial.println("RTC lost confidence in the DateTime!");
	}

	if ( ! rfid.PICC_IsNewCardPresent()) {
		return; 
	}
	if ( ! rfid.PICC_ReadCardSerial()) {
		return; 
	}

	uint32_t current_card_id = convertInt(rfid.uid.uidByte);
	Serial.print("Current card id: ");
	Serial.println(current_card_id);

	if (isMaster(current_card_id)) {
		Serial.println("Master card detected. You can add your card now.");

		long time = millis();
		// Check to see if addNewCard succseded
		bool succ = false;
		// LED on
		digitalWrite(LED_PIN, HIGH); 
		while (millis() - time < 5000){
			if ( ! rfid.PICC_IsNewCardPresent()) {
				continue; 
			}
			if ( ! rfid.PICC_ReadCardSerial()) {
				continue; 
			}

			current_card_id = convertInt(rfid.uid.uidByte);

			if(!isIdRegistered(current_card_id) && !isMaster(current_card_id)) {
				addNewCard(current_card_id);
        succ = true;
				break;
			}
			else if(isIdRegistered(current_card_id)) {
				Serial.println("Card Already Exists!");
				succ = true;
				break;      
			}      
		}
		// LED off
		digitalWrite(LED_PIN, LOW);
		if (!succ){
			Serial.println("Time ran out.");
		}

	}
	else {
		normalMode();
	}

	// Halt PICC
	rfid.PICC_HaltA();
	// Stop encryption on PCD
	rfid.PCD_StopCrypto1();

}
// End loop

bool isMaster(uint32_t id){
	if (convertInt(rfid.uid.uidByte) == master){
		return true;
	}
	return false;
}

void addNewCard(uint32_t id){
	if(addNewId(convertInt(rfid.uid.uidByte))){
		Serial.print("New card added. Id: ");
    Serial.println(convertInt(rfid.uid.uidByte));
	}
	else{
		Serial.println("Max Id count reached!");
	}

  delay(500);  
}

void normalMode(){
	RtcDateTime now = Rtc.GetDateTime();
	uint32_t timeStamp = RtcDateTime(now);
	if(isIdRegistered(convertInt(rfid.uid.uidByte))) {
		if(addNewLog(convertInt(rfid.uid.uidByte), timeStamp)){
		  Serial.println("Id found. ENTER");
		}
    else{
			Serial.println("Max Log count reached!");
    }
	}
	else {
		Serial.println("Id not found. DO NOT ENTER");
	}

	delay(500);
}

uint32_t convertInt(byte* id) {
	uint32_t id_int = 0;
	for (byte i=0; i<3; i++){
		id_int |= id[i];
		id_int <<= 8;
	}
	id_int |= id[3];
	return id_int;
}

short getIdTailIndex() {
	short tail = EEPROM_read_short(0); 
  	return tail; 
}

byte getIdIndex(uint32_t iid) {
	short tail = EEPROM_read_short(0);
	byte index = 1;
	int itr = 2;
	uint32_t readId;
	while (itr < tail)
	{
		readId = EEPROM_read_uint32(itr);
		if (readId == iid){
			return index;
		}
		itr = itr + 4;
		index = index + 1;
	}
}

uint32_t getIdByIndex(byte index){
	int itr = 4 * (index - 1) + 2;  
	return (EEPROM_read_uint32(itr));
}

bool addNewId(uint32_t iid){
	short tail = EEPROM_read_short(0);
	if (tail >= LOG_START){
    return false;		
	}
  EEPROM_write_uint32(tail, iid);
  tail = tail + 4;
  EEPROM_write_short(0, tail);
  return true;
}

short getLogTailIndex() {
	short tail = EEPROM_read_short(LOG_START);
	return tail; 
}

bool addNewLog(uint32_t iid, uint32_t timeStamp){
	byte id_index = getIdIndex(iid);
	short tail = getLogTailIndex();
	if(tail <= 1019){
		EEPROM.write(tail, id_index);
		tail = tail + 1;
		EEPROM_write_uint32(tail, timeStamp);
		tail = tail + 4;
		EEPROM_write_short(LOG_START, tail);
		return true;
	}
	return false;
}

bool isIdRegistered(uint32_t iid) {
	short tail = EEPROM_read_short(0);
	int index = 2;
	while(index < tail){
		uint32_t readId;
		readId = EEPROM_read_uint32(index);
    if (readId == iid){
			return true;
		}
		index = index + 4;
	}
	return false;
}

void resetAll() {
	short shrt = 2;
	EEPROM_write_short(0, shrt);
	shrt = LOG_START + 2;
	EEPROM_write_short(LOG_START, shrt);
  Serial.println("Total Reset Done.");
}
void resetId(){
  short shrt = 2;
	EEPROM_write_short(0, shrt);
  Serial.println("Id Reset Done.");
}
void resetLog(){
	short shrt = LOG_START + 2;
	EEPROM_write_short(LOG_START, shrt);
  Serial.println("Log Reset Done.");  
}
void clearEEPROM(){
  for(int i=0; i < EEPROM.length(); i++){
    EEPROM.write(i, 0);    
  }   
}

void showLog() {
	Serial.println("Log:");
	Serial.println("--------------------------------");
	int tail = getLogTailIndex();
	int itr = LOG_START + 2;
	byte count = 1;
	while(itr < tail){
		uint32_t id;
    uint32_t timeStamp;
    byte index = EEPROM.read(itr);    
    id = getIdByIndex(index);
    itr = itr + 1;
		timeStamp = EEPROM_read_uint32(itr);
		itr = itr + 4;
		RtcDateTime dt = RtcDateTime(timeStamp);
		char datestring[40];
		snprintf_P(datestring, 
			countof(datestring),
			PSTR("%04u/%02u/%02u %02u:%02u:%02u"),
			dt.Year(),
			dt.Month(),
			dt.Day(),
			dt.Hour(),
			dt.Minute(),
			dt.Second() );
    char buffer[3];
    sprintf(buffer, "%0.3d", count);
    Serial.print(buffer);
    Serial.print(") ");
    Serial.print(id);
    Serial.print(" @ ");
		Serial.println(datestring);
		count++;
	}
	Serial.println("--------------------------------");
}

void showId() {
	Serial.println("Id:");
	Serial.println("--------------------------------");
	int tail = getIdTailIndex();
	int itr = 2;
	byte count = 1;
	while(itr < tail){
		uint32_t id;
    id = EEPROM_read_uint32(itr);
    char buffer[3];
    sprintf(buffer, "%0.3d", count);
    Serial.print(buffer);
    Serial.print(") ");
    Serial.println(id);
		itr = itr + 4;
		count++;
	}
	Serial.println("--------------------------------");
}

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}


void EEPROM_write_uint32(int index, uint32_t value){
  byte result[4];
  
  result[0] = 0x00 | (value >> 24);
  result[1] = 0x00 | (value >> 16);
  result[2] = 0x00 | (value >> 8);
  result[3] = 0x00 | (value);

  for(byte i = 0; i < 4; i++){
      EEPROM.write(index+i,  result[i]);
  }
}

uint32_t EEPROM_read_uint32(int index){
  byte result[4];
  for(int i = 0; i < 4; i++){
    result[i] = EEPROM.read(index + i);
  }
  uint32_t id_int = 0;
	for (byte i=0; i<3; i++){
		id_int |= result[i];
		id_int <<= 8;
	}
	id_int |= result[3];
  
	return id_int;
}

void EEPROM_write_short(int index, short value){
  byte result[2];
  
  result[0] = 0x00 | (value >> 8);
  result[1] = 0x00 | (value);

  for(byte i = 0; i < 2; i++){
      EEPROM.write(index+i,  result[i]);
  }
}

short EEPROM_read_short(int index){
	byte result[2];
	for(int i = 0; i < 2; i++){
	  result[i] = EEPROM.read(index + i);
	}
	short id_short = 0;
	id_short |= result[0];
	id_short <<= 8;
	id_short |= result[1];
  
	return id_short;
}