// include the library code:
#include <EEPROM.h>
#include <Wire.h>   //i2c interface
#include <LiquidCrystal_I2C.h> //lcd through pcf8574ap
#include <OneWire.h> //onewire for temp
#include <DallasTemperature.h> //temp sensor
#include "RTClib.h"

LiquidCrystal_I2C lcd(0x38,16,2);  // set the LCD address to 0x38 for a 16 chars and 2 line display

RTC_DS1307 rtc;

int sensorPin = A0;
int sensorValue = 0;
float outputA0 = 0;
float currPh=0;
volatile byte flag = 0;//false;
byte flag_last=0;



volatile boolean light = true;
boolean light_last = true;

/*unsigned int phL = 120;
byte phLlow = phL & 0xff;
byte phLhigh = (phL >> 8);
unsigned int phH = 600;
byte phHlow = phH & 0xff;
byte phHhigh = (phH >> 8);*/
int phL=EEPROM.read(0)*256+EEPROM.read(1);
int phLv=EEPROM.read(4)*256+EEPROM.read(5);
int phH=EEPROM.read(2)*256+EEPROM.read(3);
int phHv=EEPROM.read(6)*256+EEPROM.read(7);

// Setup a oneWire instance to communicate with any OneWire devices on Pin8
OneWire oneWire(8);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Assign the addresses of your 1-Wire temp sensors.
DeviceAddress T1 = { 0x28, 0x22, 0xAA, 0x68, 0x05, 0x00, 0x00, 0xF1 };

void setup() {
  
  Serial.begin(9600);
  // set up the LCD's number of columns and rows: 
  lcd.init();  // initialize the lcd 
  lcd.noBacklight(); //inverse because npn used
  
  attachInterrupt(0, swap, RISING);
  //attachInterrupt(1, togglebl, RISING);
  
  Wire.begin();
  rtc.begin();

  //if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(__DATE__, __TIME__));
  //}  
  
  // Start up the library
  sensors.begin();
  // set the resolution to 10 bit (good enough?)
  sensors.setResolution(T1, 10);
  
  //pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(4, INPUT);
  
  lcd.clear();
}

void loop() {
  Serial.println(flag);
  
  switch (flag) {
    case 0:
      showTimeScreen();
      break;
    case 1:
      showReadingsScreen();
//      calibrate();
//      saveOnEEPROM();
      break;
    case 2:
      showMenu();
      break;
  }
  
  if (flag_last!=flag){
    lcd.clear();
    flag_last=flag;
  }
  
  if (light_last!=light){
    if (light){
      lcd.noBacklight(); //inverse because npn used
    } else {
      lcd.backlight(); //inverse because npn used
    }
  }
  light_last=light;
}

void swap()
{
  flag=flag++;
  flag=flag % 3;
}
void togglebl()
{
  light=!light;
}

void showTimeScreen()
{
  DateTime now = rtc.now();
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(1, 0);
  //    lcd.print(now.year(), DEC);
  //    lcd.print('/');
  //    if (now.day() <10) {lcd.print('0');}
  lcd.print(now.day(), DEC);
  lcd.print('/');
  //    if (now.month() <10) {lcd.print('0');}
  lcd.print(now.month(), DEC);
  lcd.setCursor(7, 0);
  if (now.hour() <10) {lcd.print('0');}
  lcd.print(now.hour(), DEC);
  lcd.print(':');
  if (now.minute() <10) {lcd.print('0');}
  lcd.print(now.minute(), DEC);
  lcd.print(':');
  if (now.second() <10) {lcd.print('0');}
  lcd.print(now.second(), DEC);
  
  lcd.setCursor(1, 1);
  sensorValue = analogRead(sensorPin);
  currPh=0.01*mapfloat(sensorValue,phL,phH,phLv,phHv);
  sensors.requestTemperatures();
  float tempC = sensors.getTempC(T1);
  lcd.print(tempC,1);
  lcd.print((char)223);
  lcd.print("C  ");
  lcd.print(currPh,2);
  lcd.print("Ph");
  //delay(1000);
}

void showReadingsScreen()
{
      lcd.setCursor(1, 0);
    sensors.requestTemperatures();
    float tempC = sensors.getTempC(T1);
    
    if (tempC == -127.00) {
      lcd.print("Error Temp");
    } else {
      lcd.print("Temp: ");
      lcd.print(tempC,2);
      lcd.print(" ");
      lcd.print((char)223);
      lcd.print("C");
    }
    lcd.setCursor(1, 1);
    // print the number of seconds since reset:
    //lcd.print(millis()/1000);
    //lcd.setCursor(3, 1);
    lcd.print("Ph  :  ");
  //  lcd.setCursor(6, 1);
    // print the number of seconds since reset:
    //lcd.print(sensorValue);
    sensorValue = analogRead(sensorPin);
    outputA0=mapfloat(sensorValue,0,1023,0,490)/100;
    //outputA0=((float)sensorValue/1024)*5;
    lcd.print(outputA0,2);
    lcd.print(" V");
    delay(500);
    //lcd.clear();
  }
  
void showMenu()
{
  lcd.setCursor(0, 0);
  lcd.print("MENU");
  
}

void calibrate()
{
  int phcL=400;
  int phcH=700;
  int reading = 0;
  int i;
  detachInterrupt(0);
  boolean brk = true;
  boolean bu = false;
  boolean bd = false;
  unsigned long curMillis = 0;
  unsigned long prevMillis = 0;
  int phcLv = 0;
  int phcHv = 0;
  
  // Input - Ph of buffer 1 (lower)
  lcd.setCursor(0, 0);
  lcd.print("Set lower Ph  ");
  lcd.setCursor(0, 1);
  lcd.print((float)phcL/100);
  delay(1000);
  while (brk){
    bu=digitalRead(3);
    bd=digitalRead(4);
    if (bu){
      phcL++;
      lcd.setCursor(0, 1);
      lcd.print((float)phcL/100);
    }
    if (bd){
      phcL--;
      lcd.setCursor(0, 1);
      lcd.print((float)phcL/100);
    }
    delay(100);
    brk=!digitalRead(2);
  }
  // Wait 10s and read for 5 sec (10 readings), take average
  lcd.setCursor(0, 0);
  lcd.print("Wait 10s    ");
  for (int i=1;i<10; i++){
    lcd.setCursor(0, 1);
    lcd.print(10-i);
    lcd.print("s left");
    delay(1000);
  }
  lcd.clear();
  reading=0;
  for (i=1; i<21; i++){
    reading=reading+analogRead(A1); //A1 temporary, back to A0
    delay(250);
    lcd.setCursor(0, 0);
    lcd.print("Reading   ");
    lcd.print(round((float)reading/i));
    lcd.setCursor(0, 1);
    if (20-i<10) {
      lcd.print(" ");
    }
    lcd.print(20-i);    
  }
  phcLv=round(reading/20);
  
  // Input - Ph of buffer 2 (higher)
  lcd.setCursor(0, 0);
  lcd.print("Set higher Ph   ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print((float)phcH/100);
  delay(1000);
  brk=!digitalRead(2);
  while (brk){
    bu=digitalRead(3);
    bd=digitalRead(4);
    if (bu){
      phcH++;
      lcd.setCursor(0, 1);
      lcd.print((float)phcH/100);
    }
    if (bd){
      phcH--;
      lcd.setCursor(0, 1);
      lcd.print((float)phcH/100);
    }
    delay(100);
    brk=!digitalRead(2);
  }
  // Wait 10s and read for 5 sec (10 readings), take average
  lcd.setCursor(0, 0);
  lcd.print("Wait 10s        ");
  for (int i=1;i<10; i++){
    lcd.setCursor(0, 1);
    lcd.print(10-i);
    lcd.print("s left");
    delay(1000);
  }
  lcd.clear();
  reading=0;
  for (i=1; i<21; i++){
    reading=reading+analogRead(A0);
    delay(250);
    lcd.setCursor(0, 0);
    lcd.print("Reading   ");
    lcd.print(round((float)reading/i));
    lcd.setCursor(0, 1);
    if (20-i<10) {
    lcd.print(" ");
    }
    lcd.print(20-i); 
  }
  phcHv=round(reading/20);  


  //check for correctness of data
  //TODO


  reading=1000; //reuse

  brk=true;
 while (brk){
    
// Show Results  
  lcd.setCursor(0, 0);
  lcd.print("Old: ");
  lcd.print((float)(EEPROM.read(0)*256+EEPROM.read(1))/100);
  lcd.print("Ph ");
  lcd.print(EEPROM.read(4)*256+EEPROM.read(5));
  lcd.setCursor(0, 1);
  lcd.print("New: ");
  lcd.print((float)phcL/100);
  lcd.print("Ph ");
  lcd.print(phcLv);
  delay(5000);
  lcd.setCursor(0, 0);
  lcd.print("Old: ");
  lcd.print((float)(EEPROM.read(2)*256+EEPROM.read(3))/100);
  lcd.print("Ph ");
  lcd.print(EEPROM.read(6)*256+EEPROM.read(7));
  lcd.setCursor(0, 1);
  lcd.print("New: ");
  lcd.print((float)phcH/100);
  lcd.print("Ph ");
  lcd.print(phcHv);
  delay(5000);

  i=1;
  prevMillis = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Show again?");
  brk=false;
  while ( (i<6) ^ (brk) ){
    curMillis = millis();
    if (curMillis-prevMillis>reading){
      prevMillis=curMillis;
      lcd.setCursor(0, 1);
      lcd.print(5-i);
      lcd.print("s left (NO) ");      
      i++;
    }
    brk=digitalRead(2);
  } 
 }
 
 delay(1000); 
  
// Saving  
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("Keep?");
  lcd.setCursor(0, 1);
  lcd.print("5s left (NO) ");
  
  i=1;
  prevMillis = millis();
  brk= false;
  while ( (i<6) ^ (brk) ){
    curMillis = millis();
    if (curMillis-prevMillis>reading){
      prevMillis=curMillis;
      lcd.setCursor(0, 1);
      lcd.print(5-i);
      lcd.print("s left (NO) ");      
      i++;
    }
    brk=digitalRead(2);

  }
  if (brk){
      lcd.setCursor(0, 1);
      lcd.print("SAVING          ");
      phL=phcL;
      phLv=phcLv;
      phH=phcH;
      phHv=phcHv;
      delay(2000);
      lcd.setCursor(0, 0);
      lcd.print("Kept, note: use");
      lcd.setCursor(0,1);
      lcd.print("Save EEPROM too");
      
  } else {
      lcd.setCursor(0, 1);
      lcd.print("Discarded   ");
  }
  attachInterrupt(0, swap, RISING);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Done        ");
  delay(2000);
  //Placeholder
/*

  delay(5000);
  
*/
}

void saveOnEEPROM()
{
  detachInterrupt(0);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Save to EEPROM?");
  unsigned long curMillis;  
  int  i=1; int interv=1000;
  boolean brk= false;
  unsigned long prevMillis = millis();
  while ( (i<11) ^ (brk) ){
    curMillis = millis();
    if (curMillis-prevMillis>interv){
      prevMillis=curMillis;
      lcd.setCursor(0, 1);
      lcd.print(10-i);
      lcd.print("s left (NO) ");      
      i++;
    }
    brk=digitalRead(2);
  }
  if (brk) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Saving calibration");
    lcd.setCursor(0, 1);
    lcd.print("to EEPROM");
    byte a=0;
    byte b=0;
    // lower PH value
    a = phL & 0xff;
    b = (phL >> 8);
    EEPROM.write(0,b);
    EEPROM.write(1,a);
    // higher PH value
    a = phH & 0xff;
    b = (phH >> 8);
    EEPROM.write(2,b);
    EEPROM.write(3,a);
    // lower PH reading
    a = phLv & 0xff;
    b = (phLv >> 8);
    EEPROM.write(4,b);
    EEPROM.write(5,a);
    // higher PH reading
    a = phHv & 0xff;
    b = (phHv >> 8);
    EEPROM.write(6,b);
    EEPROM.write(7,a);
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EEPROM saved"); 
    delay(2000); 
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EEPROM save"); 
    lcd.setCursor(0, 1);
    lcd.print("cancelled"); 
    delay(2000); 
  }
  attachInterrupt(0, swap, RISING);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Done        ");
  delay(2000);
}

float mapfloat(long x, long in_min, long in_max, int out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}
