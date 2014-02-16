// include the library code:
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

float mapfloat(long x, long in_min, long in_max, int out_min, long out_max)
{
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}
