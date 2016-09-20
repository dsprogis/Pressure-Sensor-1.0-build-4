/* *************************************************************************
 *  Pressure Sensor Project
 *  Goal:   compare pressure readings under water to establish potential
 *          from one body of water to another
 *  Issues: sensors will be at bottom measuring water above - bottom can
 *          vary in depth which requires a different reference point
 */
#include <SD.h>
#include "RTClib.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>

// Set the pins used
#define cardSelect 4
#define VBATPIN A9

// Set the error codes
#define ERROR_NO_CD_CARD        3
#define ERROR_CANT_OPEN_FILE    4 
#define ERROR_PRESSURE_READING  5      
#define ERROR_PRESSURE_SENSOR   6
#define ERROR_RTC_FAIL          7  

// Set the Device ID
#define DeviceID "00"

// Log interval in seconds and supporting counter
#define INTERVAL 2
long int interval = 0;
    
File logfile;
RTC_DS3231 rtc;
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

int clockPin = 20;
int tick = LOW;
int lastTick = LOW;


// call back for file timestamps
void dateTime(uint16_t* date, uint16_t* time) {
  DateTime now = rtc.now();
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(now.year(), now.month(), now.day());
  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}


// blink count times
void blink(uint8_t count) {
    uint8_t i;
    for (i=0; i<count; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
    }
}

// blink out an error code
void error(uint8_t errno) {
  while(1) {
    blink( errno );
    delay(2000);
  }
}

void logData() {
  DateTime now = rtc.now();
  int year = now.year();
  int month = now.month();
  int day = now.day();
  int hour = now.hour();
  int minute = now.minute();
  int second = now.second();

  String logentry;

  // Get a new sensor event
  sensors_event_t event;
  bmp.getEvent(&event);
 
  logentry = "{";

  // Log Device ID
  logentry += "\"id\": \"";
  logentry += DeviceID;
  logentry += "\", ";

  // Log DateTime
  logentry += "\"unixtime\": \"";
  logentry += now.unixtime();
  logentry += "\", ";

  // Log the Date
  logentry += "\"date\": \"";
  logentry += year;
  logentry += '-';
  if (month<10) logentry += '0';
  logentry += month;
  logentry += '-';
  if (day<10) logentry += '0';
  logentry += day;
  logentry += 'T';
  if (hour<10) logentry += '0';
  logentry += hour;
  logentry += ':';
  if (minute<10) logentry += '0';
  logentry += minute;
  logentry += ':';
  if (second<10) logentry += '0';  
  logentry += second;
  logentry += "\", ";

  // Display the results (barometric pressure is measure in hPa)
  if (event.pressure)
  {
    // Log atmospheric pressue in hPa
    logentry += "\"hPa\": \"";
    logentry += event.pressure;
    logentry += "\", ";

    // Log the temperature in C
    float temperature;
    bmp.getTemperature(&temperature);
    logentry += "\"temp_C\": \"";
    logentry += temperature;
    logentry += "\", ";
    
    // Log the temperature in F
    logentry += "\"temp_F\": \"";
    logentry += temperature*(9.0/5.0)+32.0;
    logentry += "\", ";

    // Then convert the atmospheric pressure, and SLP to altitude
    // Update this next line with the current SLP for better results
    float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
    logentry += "\"alt_m\": \"";
    logentry += bmp.pressureToAltitude(seaLevelPressure, event.pressure);
    logentry += "\", ";
    
  }
  else
  {
    Serial.print("Sensor error.");
    error( ERROR_PRESSURE_READING );
  }

  // Log battery voltage
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  logentry += "\"volt\": \"";
  logentry += measuredvbat;
  logentry += "\"";

  logentry += "}";

  Serial.println( logentry );
  logfile.println( logentry );
  logfile.flush();

  blink(1);
}

void setup() {
//  Serial.begin(115200);
  Serial.println("\r\nAnalog logger test");
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialise pressure sensor
  if(!bmp.begin())
  {
    Serial.print("Error, BMP180 not detected ... Check your wiring or I2C ADDR!");
    error( ERROR_PRESSURE_SENSOR );
  }

  // Initialize SD card
  if (!SD.begin(cardSelect)) {
    Serial.println("SD Card could not be initialized - please try re-inserting.");
    error( ERROR_NO_CD_CARD );
  }
  
  // Create a new log file on the SD card
  char filename[15];
  strcpy(filename, "ID00_V00.TXT");
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i/10;
    filename[7] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }

  // Confirm the real time clock is working
  if (!rtc.begin()) {
    Serial.println("RTC failed");
    error( ERROR_RTC_FAIL );
  };
  // RTC lost power, lets set the time
  if (rtc.lostPower()) {
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  // set date time callback function
  SdFile::dateTimeCallback(dateTime);
 
  // Open the new file
  logfile = SD.open(filename, FILE_WRITE);
  if( ! logfile ) {
    Serial.print("Couldnt create log file: "); 
    Serial.println(filename);
    error( ERROR_CANT_OPEN_FILE );
  }
  Serial.print("Writing to log file: "); 
  Serial.println(filename);

  // Set the clock pulse from the RTC
  pinMode(clockPin, INPUT_PULLUP);    // Set the switch pin as input
  rtc.writeSqwPinMode( DS3231_SquareWave1Hz );

  // Start logging on an even minute
  DateTime now = rtc.now();
  Serial.println( "Starting in ..." );
  bool wait = true;
  do {
    now = rtc.now();
    Serial.println( 60 - now.second() );
  } while( now.second() != 0 );
  
}

void loop() {

  tick = digitalRead(clockPin);   // read input value and store it in val
  if (lastTick == LOW) {
    if (tick == HIGH) {               // check if the button is pressed
      Serial.print("Interval: "); 
      Serial.println(interval);
      if(interval <= 0) {
        interval = INTERVAL;
        logData();
      }
      interval -= 1;
    }
  }
  lastTick = tick;
}
