// include the library code
#include <EEPROM.h>
#include <Wire.h>   //i2c interface
#include <LiquidCrystal_I2C.h> //lcd through pcf8574ap
#include <OneWire.h> //onewire for temp
#include <DallasTemperature.h> //temp sensor
#include "RTClib.h"
#include <SPI.h>
#include <SdFat.h>
#include <avr/io.h>
#include <avr/interrupt.h>


SdFat sd;
SdFile sdLog;
char dateb[20];
char sValb[4];

LiquidCrystal_I2C lcd(0x38, 4, 5, 6, 0, 1, 2, 3, 7, POSITIVE); // addr, EN, RW, RS, D4, D5, D6, D7, BacklightPin, POLARITY

RTC_DS1307 rtc;

OneWire oneWire(5); // Setup a oneWire instance to communicate with any OneWire devices on Pin5
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature. 
DeviceAddress T1 = { 0x28, 0x22, 0xAA, 0x68, 0x05, 0x00, 0x00, 0xF1 }; // Assign the addresses of your 1-Wire temp sensors.

volatile byte flag = 0;
volatile byte seconds = 0;
byte flag_last=0;
volatile boolean light = true;
boolean light_last = true;
boolean logging = true;


int sensorPin = A0;

/*unsigned int phL = 120;
byte phLlow = phL & 0xff;
byte phLhigh = (phL >> 8);
unsigned int phH = 600;
byte phHlow = phH & 0xff;
byte phHhigh = (phH >> 8);*/
/*
int phL=EEPROM.read(0)*256+EEPROM.read(1);
int phLv=EEPROM.read(4)*256+EEPROM.read(5);
int phH=EEPROM.read(2)*256+EEPROM.read(3);
int phHv=EEPROM.read(6)*256+EEPROM.read(7);
*/
//temporary until EEPROM fixed
int phL=120; int phLv=400; int phH=600; int phHv=700;



void setup() {
  Wire.begin();
  rtc.begin();
  sensors.begin();
  sensors.setResolution(T1, 10);// set the resolution to 10 bit (good enough?)
  //Serial.begin(9600);
  lcd.begin(16,2);
  lcd.setBacklight(BACKLIGHT_ON);

  pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(4, INPUT);
 
  if (! rtc.isrunning()) {
    //  Serial.println("RTC is NOT running!");
    //  following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }
  
  lcd.clear();
  
  // initialize Timer1
  cli();          // disable global interrupts
  
  EICRA |= (1 << ISC01);    // set INT0 to trigger on failing edge
  EIMSK |= (1 << INT0);     // Turns on INT0
  
  TCCR1A = 0;     // set entire TCCR1A register to 0
  TCCR1B = 0;     // same for TCCR1B
 
  // set compare match register to desired timer count:
  // 1s@16MHz/1024 = 15624 
  // 4s@16MHz/1024 = 62496
  OCR1A = 62496;
  // turn on CTC mode:
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler:
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);
  // enable timer compare interrupt:
  TIMSK1 |= (1 << OCIE1A);
  // enable global interrupts:

  sei();
   
  //attachInterrupt(0, swap, RISING);
}

void loop() {
  //Serial.println(flag);
  
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
//      swap();
      break;
    case 10:
//      showSD();
//      logDataShow();
      if (logging) logSD();
      flag=0;
      break;
    default:
      flag=0;
      break;
  }
  
  /* 
  if (flag_last!=flag){
    lcd.clear();
    flag_last=flag;
  }
  
  if (light_last!=light){
    if (light){
      lcd.setBacklight(BACKLIGHT_ON);
    } else {
      lcd.setBacklight(BACKLIGHT_OFF);
    }
  }
  light_last=light;
  */
}

ISR(TIMER1_COMPA_vect)
{
    seconds++;
    if (seconds == 15) //every minute i.e. 60s / 4s per interrupt
    {
        seconds = 0;
        flag=10;
    }
} 


ISR (INT0_vect)
{
  flag++;
  flag=flag % 3;
  //lcd.clear();
    /* interrupt code here */
}

void swap()
{
  flag++;
  flag=flag % 3;
  lcd.clear();
  delay(150);
}

void togglebl()
{
  light=!light;
}

void showTimeScreen()
{
  DateTime now = rtc.now();
  sprintf(dateb,  "%02d/%02d   %02d:%02d:%02d", now.day(), now.month(), now.hour(), now.minute(), now.second() );
  lcd.setCursor(0, 0);
  lcd.print(dateb);
  lcd.setCursor(1, 1);
  sensors.requestTemperatures();
  lcd.print(sensors.getTempC(T1),1);
  lcd.print((char)223);
  lcd.print("C  ");
  lcd.print(0.01*mapfloat(analogRead(sensorPin),phL,phH,phLv,phHv),2);
  lcd.print("Ph");
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
    lcd.print("Ph  :  ");
    lcd.print(mapfloat(analogRead(sensorPin),0,1023,0,500)/100,2);
    lcd.print(" V");
    delay(500);
  }
  
void showMenu()
{
  lcd.setCursor(0, 0);
  lcd.print("MENU");
  
}


String logData() {
  String dlog;
  DateTime now = rtc.now();
  float sVal = 0.01*mapfloat(analogRead(sensorPin),phL,phH,phLv,phHv);
  sensors.requestTemperatures();
  sprintf(dateb,  "%d,%d,%d,%d,%d,%d,", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second() );
  dlog=String(dateb);
  dtostrf(sVal,1,2,sValb);
  dlog=dlog + String(sValb);
  dlog = dlog + ",";
  dtostrf(sensors.getTempC(T1),1,2,sValb);
  dlog=dlog + String(sValb);
  return String(dlog); 
}

void logSD() {
  lcd.clear();
  lcd.setCursor(0, 0);  
  lcd.print("INIT SD");
  //delay(500);
  // Initialize SdFat
  if (!sd.begin(10, SPI_QUARTER_SPEED)) {
    lcd.print(" - error");
    delay(5000);
  } else {
    // if initialized correctly open the file for write at end like the Native SD library
    if (!sdLog.open("datalog.txt", O_RDWR | O_CREAT | O_AT_END)) {
      lcd.setCursor(0, 0);
      lcd.print("SD write error  ");
      delay(5000);
    } else {
      // if the file opened okay, write to it:
      lcd.setCursor(0, 1);
      lcd.print("Writing...");
      sdLog.println(logData());
      //delay(1000);
      // close the file:
      sdLog.close();
      lcd.clear();
      lcd.print("SD Writing done");
      delay(2000);
    }
  }
}

float mapfloat(long x, long in_min, long in_max, int out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

void calibrate()
{
  int phcL=400;
  int phcH=700;
  int reading = 0;
  int i;
  //detachInterrupt(0);
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
  //attachInterrupt(0, swap, RISING);
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
  //detachInterrupt(0);
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
  //attachInterrupt(0, swap, RISING);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Done        ");
  delay(2000);
}

/*void logDataShow() { //debug only
  lcd.clear();
  delay(1000);
  lcd.print(logData());
  for (int positionCounter = 0; positionCounter < 16; positionCounter++) {
    lcd.scrollDisplayLeft(); 
    delay(150);
  }
  delay(10000);
}*/

/*
void showSD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  delay(1000);
  lcd.print("INIT SD");
  // Initialize SdFat or print a detailed error message and halt
  // Use half speed like the native library.
  // change to SPI_FULL_SPEED for more performance.
  if (!sd.begin(10, SPI_QUARTER_SPEED)) sd.initErrorHalt();

  // open the file for write at end like the Native SD library
  if (!sdLog.open("test.txt", O_RDWR | O_CREAT | O_AT_END)) {
    lcd.print("write error");
    delay(10000);
    return;
  }
  // if the file opened okay, write to it:
  lcd.setCursor(0, 1);
  lcd.print("Writing...");
  sdLog.println("testing new");
  delay(1000);
  // close the file:
  sdLog.close();
  lcd.print("done.");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0,0);
  delay(1000);
  lcd.print("Reading");
  // re-open the file for reading:
  if (!sdLog.open("test.txt", O_READ)) {
    lcd.print("- open error");
  }
  delay(2000);
  lcd.setCursor(0,0);
  lcd.print("test.txt:      ");
  int i=0;
  // read from the file until there's nothing else in it:
  char data;
  char output[15];
  while ((data = sdLog.read()) >= 0) {
    if( data =='\n' || i>16) {
       lcd.setCursor(0,1);
       lcd.print(output);
       lcd.setCursor(i-1,1); lcd.print("                ");
       delay(1000);
       if (i>15) {output[0]=data; lcd.setCursor(i,1); lcd.print("                "); i=1;} 
       else {lcd.setCursor(i-1,1); lcd.print("                "); i=0;}
    } else {
       output[i]=data;
       i++;
    }
  }
  // close the file:
  sdLog.close();
  lcd.setCursor(0,0);
  lcd.print("FINISHED      "); 

  delay(2000); 
}*/
