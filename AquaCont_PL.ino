// include the library code
#include <EEPROM.h>             // save/read EEPROM
#include <Wire.h>               //i2c interface
#include <LiquidCrystal_I2C.h>  //lcd through pcf8574ap
#include <OneWire.h>            //onewire for temp
#include <DallasTemperature.h>  //temp sensor
#include "RTClib.h"             // RTC for DS1370
#include <SPI.h>                // SPI interface for SD card
#include <SdFat.h>              // SD card handler
#include <avr/pgmspace.h>       // saving in program memory

#define BTIME 100    //debouncing time for buttons
#define BYTE 8       //byte size
#define TOTALCUSTCHAR 5 // total amount of custom chars
#define TOTALPOS 8   //total menu options
#define TOTALSCR 3   //total screens
#define SENSORPIN A0 // Ph sensing pin
#define LTIME 30     //internal average logging time in seconds
#define MAXSTRLEN 16 // LCD row length

//various library-related variables
SdFat sd;
SdFile sdLog;
RTC_DS1307 rtc;
DateTime now;
LiquidCrystal_I2C lcd(0x38, 4, 5, 6, 0, 1, 2, 3, 7, POSITIVE); // addr, EN, RW, RS, D4, D5, D6, D7, BacklightPin, POLARITY
OneWire oneWire(5); // Setup a oneWire instance to communicate with any OneWire devices on Pin5
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature. 
DeviceAddress T1 = { 0x28, 0x22, 0xAA, 0x68, 0x05, 0x00, 0x00, 0xF1 }; // Assign the addresses of your 1-Wire temp sensors.

// Menu options saved in program memory
static char buffer[MAXSTRLEN+1];
const char mi0[] PROGMEM = " Podswietlenie";
const char mi1[] PROGMEM = "  Logowanie SD";
const char mi2[] PROGMEM = "SD Czestotliwosc";
const char mi3[] PROGMEM = " Ph Kalibracja";
const char mi4[] PROGMEM = "Zapis Kalibracji";
const char mi5[] PROGMEM = "   Zmien Czas";
const char mi6[] PROGMEM = "   Zmien Date";
const char mi7[] PROGMEM = "     Wyjdz";

PGM_P const mi[] PROGMEM = 
{
    mi0,mi1,mi2,mi3,mi4,mi5,mi6,mi7
};

//Read calibration data from EEPROM
unsigned int phL=EEPROM.read(0)*256+EEPROM.read(1);
unsigned int phLv=EEPROM.read(4)*256+EEPROM.read(5);
unsigned int phH=EEPROM.read(2)*256+EEPROM.read(3);
unsigned int phHv=EEPROM.read(6)*256+EEPROM.read(7);

//Setup custom LCD characters
static const PROGMEM unsigned char signs[] = 
{     
   0x4,0xe,0x15,0x4,0x4,0x4,0x4,0x4,      //0 - arrow UP
   0x4,0x4,0x4,0x4,0x4,0x15,0xe,0x4,      //1 - arrow DOWN
   0x18,0x10,0x8,0x18,0x6,0x5,0x5,0x6,    //2 - SD sign 
   0x0,0x1,0x3,0x16,0x1c,0x8,0x0,0x0,     //3 - tick
   0x0,0x1b,0xe,0x4,0xe,0x1b,0x0,0x0,     //4 - cross      
}; 


volatile byte flag = 0;          // flag - screen id
volatile boolean clrscr = false; // flag - clear screen

boolean inMenu = false;     // flag - inside menu
byte menuPos = 0;           // position in menu

unsigned int avg_ph=0; unsigned int avg_temp=0; 
byte avg_count=0;           // total amount of logs summed in above
boolean SDlogging = true;   // flag - enable logging
boolean SDerror = false;    // flag - SD write error
byte logSDtime = 5;         // time [min] between SD save
char data[40];              // char buffer for main login line
char csPh[6];               // char buffer for logging ph
char csT[6];                // char buffer for logging temperature
uint32_t lastLog;           // used to time the logging

uint32_t lastButton;        //used for button debouncing in menu

void setup() {

  // Setup interrupts
  PCICR |= (1<<PCIE2);
  PCMSK2 |= (1<<PCINT20); //D4 -> 20
  MCUCR = (1<<ISC01) | (1<<ISC01);
  pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(4, INPUT);
  interrupts();
  
  // open interfaces
  Wire.begin();
  rtc.begin();
  sensors.begin();
  sensors.setResolution(T1, 12);// set the resolution to 12 bit
  sensors.requestTemperatures();
  lcd.begin(16,2);
  lcd.setBacklight(BACKLIGHT_ON);
  
  //load extra signs into LCD memory
  unsigned char buf[BYTE];
  for (unsigned char i=0; i<TOTALCUSTCHAR; i++) {
    for(unsigned char j=0; j<BYTE; j++) 
       { 
           buf[j]=pgm_read_byte_near(&signs[j+BYTE*i]); 
       }
    lcd.createChar(i, buf);
  }
  
  // set compile time if RTC not working
  if (! rtc.isrunning()) {
    //  Serial.println("RTC is NOT running!");
    //  following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }
  
  // get time
  lcd.clear();
  now = rtc.now();
  lastLog=now.unixtime();
  
  // initialize SD card
  lcd.print(F("Start SD"));
  delay(500);
  // Initialize SdFat
  if (!sd.begin(10, SPI_QUARTER_SPEED)) {
    lcd.print(F(" - blad"));
    lcd.setCursor(1,0);
    sd.initErrorPrint();
    SDerror=true;
    delay(5000);
  } else {
    now = rtc.now();
    char fname[14];
    sprintf(fname,  "log_%d_%d.csv", now.month(), now.day());
    // if initialized correctly open the file for write at end like the Native SD library
    if (!sdLog.open(fname, O_RDWR | O_CREAT | O_AT_END)) {
      lcd.setCursor(0, 0);
      lcd.print(F("SD blad pliku"));
      lcd.setCursor(1,0); SDerror=true;
      sd.initErrorPrint();
      delay(5000);
    } else {
      lcd.print(F(" OK"));
      delay(2000);
    }
  }

}


void loop() {
  // check for clearing screen
  if (clrscr) {
    lcd.clear();
    clrscr=false;
  }
  // get time
  now = rtc.now();
  if ( lastLog + LTIME < now.unixtime() ) //check if time to log
    {
    if (SDlogging) logData();
    lastLog = now.unixtime();
    } 
  else  // otherwise show screen
   {
   switch (flag) {
    case 0:
      showTimeScreen();
      break;
    case 2:
      showRAW();
      break;
    case 1:
      showMenu();
      break;
    default:
      flag=0;
      break;
   }
  }

}

// interrupt function - change screen no if NOT in menu
ISR(PCINT2_vect)
{
  if( digitalRead(4) == 1 && !inMenu) {
    flag++;
    flag=flag % TOTALSCR;
    clrscr=true;
  }
}

// show main screen
void showTimeScreen()
{
  //update time
  sprintf(data,  "%02d/%02d   %02d:%02d:%02d", now.day(), now.month(), now.hour(), now.minute(), now.second() );
  lcd.setCursor(0, 0);
  lcd.print(data);
  //update temp
  lcd.setCursor(0, 1);
  lcd.print(sensors.getTempC(T1),1);
  lcd.write((uint8_t)223);
  lcd.print("C");
  sensors.requestTemperatures(); // ask for new one
  //update Ph
  lcd.setCursor(10, 1);
  lcd.print(0.01*mapfloat(analogRead(SENSORPIN),phL,phH,phLv,phHv),2);
  lcd.print("Ph");
  //update SD status
  lcd.setCursor(7,1);
  lcd.write((uint8_t)2);
  if (SDerror) lcd.print("!");
  else if (SDlogging)  lcd.write((uint8_t)3);
       else lcd.write((uint8_t)4);
}

// show simple screen for debug & calibration
void showRAW() { 
    lcd.setCursor(0, 0);          
    lcd.print(F("Temp:  "));
    lcd.print(sensors.getTempC(T1),2);
    lcd.print(" ");
    lcd.write((uint8_t)223);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print(F("Ph  :  "));
    lcd.print(analogRead(SENSORPIN));
    sensors.requestTemperatures();
    delay(BTIME);
  }

// data logging
void logData() { 
  lcd.setCursor(5,0);
  lcd.print("*"); // heart-beat
  delay(100);
  // sum to a variable-container
  if (avg_count <= 255) { //prevent overflows
    avg_ph += analogRead(SENSORPIN); 
    avg_temp += (int) 100*sensors.getTempC(T1);
    avg_count++;
  }
  //if time to log on SD
  if (avg_count*LTIME/60 >= logSDtime) { 
    dtostrf(0.01*mapfloat((float) avg_ph/avg_count,phL,phH,phLv,phHv),1,2,csPh); // convert float Ph into char table
    dtostrf((float) 0.01*avg_temp/avg_count,1,2,csT);                            // convert float Temp into char table
    DateTime logt (now.unixtime() - 0.5*avg_count*LTIME);                        // save mid-time between SD logs
    //create saving buffer
    sprintf(data,  "%d,%d,%d,%d,%d,%d,%s,%s", logt.year(), logt.month(), logt.day(), logt.hour(), logt.minute(), logt.second(), csT, csPh );
    saveSD(); // save on SD
    avg_ph=0; avg_temp=0; avg_count=0; //reset counters
  }
  lcd.setCursor(5,0);
  lcd.print(" "); // remove heart-beat
}

// save prepared buffer "data" on SD card
void saveSD() {
  lcd.clear();
  lcd.setCursor(0, 0);  
  lcd.print(F("SD Zapis..."));
  lcd.setCursor(3,1); // show saved values, why not
  lcd.print(csT); lcd.print(F(" ")); lcd.print(csPh);
  delay(1000);
  sdLog.println(data);  // SAVE
  lcd.setCursor(8,0);
  //command to sync the SD card
  if (!sdLog.sync()) { //if failed
    
    lcd.print(F(" blad"));
    sd.errorPrint(); SDerror=true;
  } else { // success
    lcd.print(F(" OK"));
  };
  delay(1000);
  lcd.clear();
}  

// liner mapping for float values
float mapfloat(long x, long in_min, long in_max, int out_min, long out_max) {
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

// read buttons in menu
byte readButton() {
  byte button = 0;
  
  if ( lastButton + BTIME < millis() ) {
    //UP
    if (digitalRead(2)) { button=button+1; lastButton=millis(); }
    //DOWN
    if (digitalRead(3)) { button=button+2; lastButton=millis(); } 
    //SELECT
    if (digitalRead(4)) { button=button+4; lastButton=millis(); }   
  }
  return button; 
}

// shows up/down arrows at the edges of the screen
void showArrows() {
  lcd.setCursor(0,1); lcd.write((uint8_t)0); lcd.setCursor(15,1); lcd.write((uint8_t)1);
}

// shows Yes/No options at the edges of the screen
void showYESNO(){
  lcd.setCursor(0,1); lcd.print(F("TAK          NIE"));
}

// shows current menu option (title) from progmem
void menuLine(byte menuPos) {
  char lcdbuffer[MAXSTRLEN+1];
  lcd.clear();
  strcpy_P(lcdbuffer, (char*)pgm_read_word(&(mi[menuPos])));        
  lcd.print(lcdbuffer);
  showArrows();
  lcd.setCursor(7,1); lcd.print("OK");
}  

// represents int as 1/100th float
void showFloat(int number) {
  lcd.setCursor(6,1); lcd.print((int) number / 100 ); lcd.print("."); if (number % 100 < 10) lcd.print("0"); lcd.print(number % 100);
}

// show MENU screen
void showMenu() {
  lcd.setCursor(5, 0);
  lcd.print(F("MENU"));
  showArrows();
  lcd.setCursor(5,1); lcd.print(F("Wejdz"));
  delay(BTIME);
  byte a=readButton();
  if (a == 1 || a == 2) { // if Up or Down pressed - ented menu
    //prepare variables
    byte b=0; byte c=0; byte d=0; char temp[BYTE]; boolean flicker=true;
    inMenu=true; menuPos=0; // set flag
    menuLine(menuPos); // draw menu positon
    do {     
      delay(300); //debounce
      switch (readButton()) {
        case 1: // UP button
        menuPos++;
        menuPos=menuPos % TOTALPOS;
        menuLine(menuPos);
        break;
        case 2: // DOWN button   
        menuPos--;
        menuPos=menuPos % TOTALPOS;
        menuLine(menuPos);
        break;
        
        case 4: //OK button
        switch (menuPos){
          
          case 0:  //BACKLIGHT MENU
          showYESNO();
          delay(1000); a=0;
          while (a == 0) {delay(BTIME); a=readButton();}
          if (a == 1) {lcd.setBacklight(BACKLIGHT_ON);} 
          else if (a == 2) {lcd.setBacklight(BACKLIGHT_OFF);};     
          delay(500);
          menuLine(menuPos);        
          break;
          
          case 1:  //SD LOG FLAG MENU
          showYESNO();
          delay(1000); a=0;
          while (a == 0) {delay(BTIME); a=readButton();}
          if (a == 1) {SDlogging = true; avg_ph=0; avg_temp=0; avg_count=0; now = rtc.now(); lastLog = now.unixtime();}
          else if (a == 2) SDlogging = false;     
          delay(500);
          menuLine(menuPos);
          break;
         
          case 2: //SD SYNC FREQUENCY MENU
          lcd.clear();
          lcd.print(F("Ustaw czest. SD"));
          showArrows();
          lcd.setCursor(5,1); 
          if (logSDtime<10) lcd.print("0");
          lcd.print(logSDtime); lcd.print(F(" min")); 
          a=0; delay(1000);
          do {
            delay(BTIME); 
            if (a==1) logSDtime++;
            if (a==2) logSDtime--;
            logSDtime = logSDtime % 128; //max 127mins between SD sync
            lcd.setCursor(5,1);
            if (logSDtime < 10) lcd.print("0");
            lcd.print(logSDtime);
            a=readButton();
          } while (a!=4);  // OK to finish
          delay(500);
          menuLine(menuPos);
          break;
          
          case 3: // PH CALIBRATION
          lcd.clear();
          lcd.print(F("Niski Ph"));
          showArrows();
          showFloat(phLv);
          a=0; delay(1000);
          do {
            delay(BTIME); 
            if (a==1) phLv++;
            if (a==2) phLv--;
            showFloat(phLv);
            a=readButton();
          } while (a!=4) ; // OK to next
          
          lcd.clear();
          lcd.print(F("Niski Ph Odczyt"));
          showArrows();
          lcd.setCursor(6,1); lcd.print(phL);
          a=0; delay(1000);
          do {
            delay(BTIME); 
            if (a==1) phL++;
            if (a==2) phL--;
            if (phL < 100) lcd.setCursor(6,1); lcd.print(F("    "));
            lcd.setCursor(6,1); lcd.print(phL);
            a=readButton();
          } while (a!=4); // OK to next         
          
          lcd.clear();
          lcd.print(F("Wysoki Ph"));
          showArrows();
          showFloat(phHv);
          a=0; delay(1000);
          do {
            delay(BTIME); 
            if (a==1) phHv++;
            if (a==2) phHv--;
            showFloat(phHv);
            a=readButton();
          } while (a!=4) ;
          
          lcd.clear();
          lcd.print(F("Wysoki Ph Odczyt"));
          showArrows();
          lcd.setCursor(6,1); lcd.print(phH);
          a=0; delay(1000);
          do {
            delay(BTIME); 
            if (a==1) phH++;
            if (a==2) phH--;
            if (phL < 100) lcd.setCursor(6,1); lcd.print(F("    "));
            lcd.setCursor(6,1); lcd.print(phH);
            a=readButton();
          } while (a!=4);
          delay(500);
          menuLine(menuPos);
          break;
          
          case 4: //SAVE ON EEPROM 
          showYESNO();
          delay(3000); a=0;
          lcd.setCursor(5,1); lcd.print(F("Wybierz"));
          while (a == 0) {delay(BTIME); a=readButton();}
          if (a == 1) {
            a=0; b=0;
            // lower PH value
            a = phL & 0xff; b = (phL >> BYTE);
            EEPROM.write(0,b); EEPROM.write(1,a);
            // higher PH value
            a = phH & 0xff; b = (phH >> BYTE);
            EEPROM.write(2,b); EEPROM.write(3,a);
            // lower PH reading
            a = phLv & 0xff; b = (phLv >> BYTE);
            EEPROM.write(4,b); EEPROM.write(5,a);
            // higher PH reading
            a = phHv & 0xff; b = (phHv >> BYTE);
            EEPROM.write(6,b); EEPROM.write(7,a);
            lcd.setCursor(0,1); lcd.print(F("    Zapisane    "));  
          }
          else if (a == 2) lcd.setCursor(0,1); lcd.print(F("  Nie Zapisane  "));     
          delay(2000);
          menuLine(menuPos);
          break;
          
          case 5: //SET TIME
          lcd.clear();
          now = rtc.now();
          d = now.second();
          c = now.minute();
          b = now.hour();
          sprintf(temp,  "%02d:%02d:%02d", b,c,d );
          showArrows();
          lcd.setCursor(4, 1); lcd.print(temp);
          a=0; delay(1000);          
          do { // HOURS
            delay(200); 
            if (a==1) b++;
            if (a==2) b--;
            b=b % 24;
            if (flicker) sprintf(temp,  "  :%02d:%02d", c,d );
            else sprintf(temp,  "%02d:%02d:%02d", b,c,d );
            flicker=!flicker;
            lcd.setCursor(4, 1); lcd.print(temp);
            a=readButton();
          } while (a!=4);
          delay(1000);
          do { //MINS
            delay(200); 
            if (a==1) c++;
            if (a==2) c--;
            c= c % 60;
            if (flicker) sprintf(temp,  "%02d:  :%02d", b,d );
            else sprintf(temp,  "%02d:%02d:%02d", b,c,d );
            flicker=!flicker;
            lcd.setCursor(4, 1); lcd.print(temp);
            a=readButton();
          } while (a!=4);
          delay(1000);
          do { //SECONDS
            delay(200); 
            if (a==1) d++;
            if (a==2) d--;
            d= d % 60;
            if (flicker) sprintf(temp,  "%02d:%02d:  ", b,c );
            else sprintf(temp,  "%02d:%02d:%02d", b,c,d );
            flicker=!flicker;
            sprintf(temp,  "%02d:%02d:%02d", b,c,d );
            lcd.setCursor(4, 1); lcd.print(temp);
            a=readButton();
          } while (a!=4);
          showYESNO();
          lcd.setCursor(4, 0); lcd.print(temp);
          lcd.setCursor(5,1);
          lcd.print(F("Zapis?"));
          delay(500); a=0;
          while (a == 0 || a == 4) {delay(BTIME); a=readButton();}
          if (a == 1) { SDlogging = false; now = rtc.now();
            rtc.adjust(DateTime(now.year(),now.month(),now.day(), (uint8_t) b, (uint8_t) c, (uint8_t) d));
            lcd.setCursor(0,1); lcd.print(F("    Zapisane    "));
          } else lcd.setCursor(0,1); lcd.print(F("  Nie Zapisane  "));
          delay(2000);
          menuLine(menuPos);
          break;
          
          case 6: //SET DATE
          lcd.clear();
          now = rtc.now();
          b = now.day();
          c = now.month();
          d = now.year() % 2000;
          sprintf(temp,  "%02d/%02d/%02d", b,c,d );
          showArrows();
          lcd.setCursor(4, 1); lcd.print(temp);
          a=0; delay(1000);          
          do { // DAYS
            delay(200); 
            if (a==1) b++;
            if (a==2) b--;
            b = b % 32;
            if (flicker) sprintf(temp,  "  /%02d/%02d", c,d );
            else sprintf(temp,  "%02d/%02d/%02d", b,c,d );
            flicker=!flicker;
            lcd.setCursor(4, 1); lcd.print(temp);
            a=readButton();
          } while (a!=4);
          delay(1000);
          do { //MONTHS
            delay(200); 
            if (a==1) c++;
            if (a==2) c--;
            c= c % 13;
            if (c == 0) c++;
            if (flicker) sprintf(temp,  "%02d/  /%02d", b,d );
            else sprintf(temp,  "%02d/%02d/%02d", b,c,d );
            flicker=!flicker;
            lcd.setCursor(4, 1); lcd.print(temp);
            a=readButton();
          } while (a!=4);
          delay(1000);
          do { //YEARS
            delay(200); 
            if (a==1) d++;
            if (a==2) d--;
            d= d % 100;
            if (flicker) sprintf(temp,  "%02d/%02d/  ", b,c );
            else sprintf(temp,  "%02d/%02d/%02d", b,c,d );
            flicker=!flicker;
            lcd.setCursor(4, 1); lcd.print(temp);
            a=readButton();
          } while (a!=4);
          showYESNO();
          sprintf(temp,  "%02d/%02d/%02d", b,c,d );
          lcd.setCursor(4, 0); lcd.print(temp);
          lcd.setCursor(5,1);
          lcd.print(F("Zapis?"));
          delay(500); a=0;
          while (a == 0 || a == 4) {delay(BTIME); a=readButton();}
          if (a == 1) { SDlogging = false; now = rtc.now();
            rtc.adjust(DateTime((uint8_t) d+2000, (uint8_t) c, (uint8_t) b, now.hour(),now.minute(),now.second()));
            lcd.setCursor(0,1); lcd.print(F("    Zapisane    "));
          } else lcd.setCursor(0,1); lcd.print(F("  Nie Zapisane  "));
          delay(2000);
          menuLine(menuPos);
          break;
          
          case 7: //EXIT MENU
          lcd.clear();
          lcd.setCursor(0,0);
          menuLine(menuPos); 
          showYESNO();
          lcd.setCursor(5,1);
          lcd.print(F("Wyjdz?"));
          delay(1000); a=0;
          while (a == 0 || a == 4) {delay(BTIME); a=readButton();}
          if (a == 1) {inMenu = false; flag=0; clrscr=true;}
          else menuLine(menuPos);
          delay(500);
          break;
        }
        
        break; // break menu screen ( 4 - UP )
        
        default:
        break;
      }
    } while (inMenu); // loop menu case until Exit menu selected
  }
}
