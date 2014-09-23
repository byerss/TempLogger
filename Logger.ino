#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <Wire.h>
#include "RTClib.h"

RTC_DS1307 rtc;

// --- EEPROM ADDRESS DEFINITIONS
#define LCD_BACKLIGHT_ADDRESS 1  // EEPROM address for backlight setting
#define BAUD_ADDRESS 2  // EEPROM address for Baud rate setting
#define SPLASH_SCREEN_ADDRESS 3 // EEPROM address for splash screen on/off
#define ROWS_ADDRESS 4  // EEPROM address for number of rows
#define COLUMNS_ADDRESS 5  // EEPROM address for number of columns

// --- SPECIAL COMMAND DEFINITIONS
#define BACKLIGHT_COMMAND 128  // 0x80
#define SPECIAL_COMMAND 254 // 0xFE
#define BAUD_COMMAND 129  // 0x81

#define LCD_WIDTH 16
#define LCD_HEIGHT 2

// --- ARDUINO PIN DEFINITIONS
uint8_t RSPin = 2;
uint8_t RWPin = 3;
uint8_t ENPin = 4;
uint8_t D4Pin = 5;
uint8_t D5Pin = 6;
uint8_t D6Pin = 7;
uint8_t D7Pin = 8;
uint8_t BLPin = 9;

char inKey;  // Character received from serial input
uint8_t Cursor = 0;  // Position of cursor, 0 is top left, (rows*columns)-1 is bottom right
uint8_t LCDOnOff = 1;  // 0 if LCD is off
uint8_t blinky = 0;  // Is 1 if blinky cursor is on
uint8_t underline = 0; // Is 1 if underline cursor is on
uint8_t splashScreenEnable = 1;  // 1 means splash screen is enabled
uint8_t rows = 2;  // Number rows, will be either 2 or 4
uint8_t columns = 16; // Number of columns, will be 16 or 20
uint8_t characters; // rows * columns

// initialize the LCD at pins defined above
LiquidCrystal lcd(RSPin, RWPin, ENPin, D4Pin, D5Pin, D6Pin, D7Pin);

OneWire  ds(10);

  
int Day;
int Second;
int Month;
int Year;
int Hour;
int Minute;

char* date = "Sep 19 2014";
char* time = "15:57:20";

String minPad;
String secPad;
 
void setup(){
 // start serial port
 
  Serial.begin(9600);
  
  
  setBaudRate(EEPROM.read(BAUD_ADDRESS));
 
  // Read rows and columns from EEPROM
  // Will default to 2x16, if not previously set
  rows = EEPROM.read(ROWS_ADDRESS);
  if (rows != 4)
    rows = 2;
  columns = EEPROM.read(COLUMNS_ADDRESS);
  if (columns != 20)
    columns = 16;
 
  // set up the LCD's number of rows and columns:
  lcd.begin(columns, rows);
 
  // Set up the backlight
  pinMode(BLPin, OUTPUT);
  setBacklight(255);
 
  // Do splashscreen if set
  splashScreenEnable = EEPROM.read(SPLASH_SCREEN_ADDRESS);
  
  
#ifdef AVR
  Wire.begin();
#else
  Wire1.begin(); // Shield I2C pins connect to alt I2C bus on Arduino Due
#endif
  rtc.begin();

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

//rtc.adjust(DateTime((date), (time)));   //Use to Manually adjust date/time
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  //Use to adjust date/time to compile time (about 20 seconds behind)

}

      
/*----------------------------------------------------------

  ----------------------------------------------------------*/
void loop()
{
  

  
  
  
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
   // Serial.println("No more addresses.");
   // Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }
  
  //Serial.print("ROM =");
 // for( i = 0; i < 8; i++) {
   // Serial.write(' ');
   // Serial.print(addr[i], HEX);
 // }

  if (OneWire::crc8(addr, 7) != addr[7]) {
     // Serial.println("CRC is not valid!");
      return;
  }

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
     // Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
    //  Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
    //  Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
    //  Serial.println("Device is not a DS18x20 family device.");
      return;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

 // Serial.print("  Data = ");
 // Serial.print(present, HEX);
 // Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  //  Serial.print(data[i], HEX);
   // Serial.print(" ");
  }
 // Serial.print(" CRC=");
 // Serial.print(OneWire::crc8(data, 8), HEX);
 // Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;

 
 DateTime now = rtc.now();
    
    Year = (now.year());
    Month = (now.month());
    Day = (now.day());
    Hour = (now.hour());
    Minute = (now.minute());
    Second = (now.second());
    
    if( Minute < 10){
      minPad = "0"; }
    else{
      minPad = "";}
      
   if( Second < 10){
      secPad = "0"; }
    else{
      secPad = "";}   

      
    Serial.print(Year, DEC);
    Serial.print(',');
    Serial.print(Month, DEC);
    Serial.print(',');
    Serial.print(Day, DEC);
    Serial.print(',');
    Serial.print(Hour, DEC);
    Serial.print(':');
    Serial.print(minPad);
    Serial.print(Minute, DEC);
    Serial.print(':');
    Serial.print(secPad);
    Serial.print(Second, DEC);
    Serial.print(',');
    Serial.println(celsius);
    

  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  lcd.print(celsius);
  
  

  lcd.setCursor(0,1);
  lcd.print(Month); lcd.print("/"); lcd.print(Day); lcd.print("/"); lcd.print(Year);
  lcd.setCursor(11,1);
  lcd.print(Hour); lcd.print(":"); lcd.print(minPad); lcd.print(Minute);
  
  



}










void setBacklight(uint8_t backlightSetting)
{
  analogWrite(BLPin, backlightSetting);
  EEPROM.write(LCD_BACKLIGHT_ADDRESS, backlightSetting);
  //Serial.println("Backlight Setting Changed");
}

/* ----------------------------------------------------------
  setBaudRate() is called from SpecialCommands(). It receives
  a baud rate setting balue that should be between 0 and 10.
  The baud rate is then set accordingly, and the new value is
  written to EEPROM. If the EEPROM value hasn't been written
  before (255), this function will default to 9600. If the value
  is out of bounds 10<baud<255, no action is taken.
  ----------------------------------------------------------*/
void setBaudRate(uint8_t baudSetting)
{
  // If EEPROM is unwritten (0xFF), set it to 9600 by default
  if (baudSetting==255)
    baudSetting = 4;
   
  switch(baudSetting)
  {
    case 0:
      Serial.begin(300);
      break;
    case 1:
      Serial.begin(1200);
      break;
    case 2:
      Serial.begin(2400);
      break;
    case 3:
      Serial.begin(4800);
      break;
    case 4:
      Serial.begin(9600);
      break;
    case 5:
      Serial.begin(14400);
      break;
    case 6:
      Serial.begin(19200);
      break;
    case 7:
      Serial.begin(28800);
      break;
    case 8:
      Serial.begin(38400);
      break;
    case 9:
      Serial.begin(57600);
      break;
    case 10:
      Serial.begin(115200);
      break;
  }
  if ((baudSetting>=0)&&(baudSetting<=10))
    EEPROM.write(BAUD_ADDRESS, baudSetting);
    
}
