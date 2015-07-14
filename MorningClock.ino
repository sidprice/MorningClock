#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <RTC_DS3231.h>

#include <SimpleTimer.h>

#include <Adafruit_NeoPixel.h>

//
// LED Morning Clock
//
// Copyright Sid Price 2015 - All rights reserved
//

//
// This is the table of pixels on in the display, it is
// accessed by using the difference minutes
//

const unsigned char ledsOn[] = {
  8,7,7,7,7,6,6,6,6,5,5,5,5,4,4,4,4,3,3,3,3,2,2,2,2,1,1,1,1,0,0,0
} ;
//
// LED strip attached to the following pin
//
#define PIN 6
//
// Display colors
//
uint32_t colorFirstSegment ;
uint32_t colorSecondSegment ;
uint32_t colorThirdSegment ;

uint8_t ucLedOnCount ;

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, PIN, NEO_GRB + NEO_KHZ800);

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

//
// The RTC object
//
RTC_DS3231 RTC ;
//
// Simple Timer object, used for
// timing the comparison of time and alarm
//
SimpleTimer timer ;
int   iTimerID ;
#define cTimerPeriod    5000  // Expressed in milliseconds
int   iAlarmHour ;
int   iAlarmMinute ;

uint8_t ui8Bright ;

long lPostAlarmActive = 0 ;      // A counter that keps the full strip displayed for "x" ticks after alarm
long lPostAlarmCount ;
//
// Post alarm hold time
//
#define cPostAlarmTime  10L      // Expressed in minutes

//
// Define the EEPROM address usage
//

typedef enum {
  eeBright = 0,
  eeAlarmHour,        // 0x01
  eeAlarmMinute,
} eepOffsets ;

void setup() {
  //
  // initialize serial:
  //
  Serial.begin(57600);
  //
  // reserve 200 bytes for the inputString:
  //
  inputString.reserve(64);
  //
  // Calculate the numnber of timer ticks for the full display hold time
  //
  lPostAlarmCount = cPostAlarmTime * 60 ;
  lPostAlarmCount *= 1000 ;
  lPostAlarmCount /= cTimerPeriod ;
  //
  // Setup the LED strip
  //
  strip.begin();
  strip.setBrightness(128) ;
  strip.show(); // Initialize all pixels to 'off'
  //
  // Setup segment colors
  //
  colorFirstSegment = strip.Color(0,204,204) ;    // Dark green
  colorSecondSegment = strip.Color(255,255,102) ; // Kinda Yellow
  colorThirdSegment = strip.Color(255,128,0) ;  // Orange
  //
  // Get the brightness from the EEPROM
  //
  ui8Bright = EEPROM.read(eeBright) ;
  //
  if (ui8Bright != 0)
  {
    strip.setBrightness(ui8Bright) ;
    Serial.print("Brightness reset to ") ;
    Serial.println(ui8Bright) ;
  }
  else
  {
    strip.setBrightness(16) ;   // Set a default (low)
    Serial.println("Brightness set to default of 16") ;
  }
  //
  // Get the Alarm from the EEPROM
  //
  iAlarmHour = EEPROM.read(eeAlarmHour) ;
  iAlarmMinute = EEPROM.read(eeAlarmMinute) ;
  if((iAlarmHour >= 0) && (iAlarmHour <= 23))
  {
    Serial.print("Alarm == ") ;
    Serial.print(iAlarmHour,DEC) ;
    Serial.print(":") ;
    Serial.println(iAlarmMinute) ;
  }
  else
  {
    iAlarmHour = 0 ;
    iAlarmMinute = 0 ;
    Serial.println("Alarm not set!") ;
  }
  //
  // Init the Wire and RTC hardware
  //
  Wire.begin() ;
  RTC.begin() ;  
  // Check if the RTC is running.
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running");
  }
  //
  DateTime now = RTC.now();
  //
  // Print the current time to the serial port
  //
  char  szTimeBuffer[64] ;
  now.toString(szTimeBuffer,64) ;
  Serial.println(szTimeBuffer) ;
  //
  // Setup the timer
  //
  iTimerID = timer.setInterval(5000,timerCallback) ;
  Serial.print("Alarm hold time is ") ;
  Serial.println(lPostAlarmCount) ;
  Serial.println("Setup complete.");
}

//
//  Function:   getTimeAndDateFromCommand(void)
//

//
// Main Loop:
//
//  1. Reads commands from the serial port
//    a. SetTime - Sets the RTC
//    b. GetTime - Reads the RTC and sends it to the console
//    c. SetAlarm - Sets the Alarm time
//    d. GetAlarm - Gets the Alarm time
//    e. SetBright - Sets the display brightness
//          0 == off
//        255 == full intensity
//    f. GetBright - Gets the current display intensity
//    g. SetHold - Sets the alarm display hold time (expressed in minutes)
//    h. GetHold - Gets the alarm display hold time (expressed in minutes)
//  2. Reads the RTC
//    a. Checks if the TOD is within 30 minutes of the Alarm and calls appropriate processing
//

void loop() {
  timer.run() ;
  //
  //  Process any strings that arrive on the serial line
  //
  if (stringComplete)
  {
    int iError = -1 ;
    char  * lpszSyntax = "" ;
    //
    inputString.toLowerCase() ;
    if((inputString.startsWith("settime")) || (inputString.startsWith("setalarm")))
    {
      boolean isAlarm = false ;
      //
      if(inputString.startsWith("setalarm"))
      {
        isAlarm = true ;
        lpszSyntax = "SetAlarm hh:mm" ;
      }
      else
      {
        lpszSyntax = "SetTime hh:mm" ;
      }
      //
      //  Process the aruments for this command
      //
      //  SetTime hh:mm
      //    The seconds are implicitly set to "00" we
      //    don't really care about any better accuracy!
      //
      int anIndex ;
      //
      // Get the index of the space between the command and the parameters
      //
      anIndex = inputString.indexOf(' ') ;
      //
      // If this fails we are done
      //
      if(anIndex > 0)
      {
        //
        // Get the parameters
        //
        int iComma = inputString.indexOf(':') ;
        if(iComma > 0)
        {
          String  stringHours ;
          String  stringMinutes ;
          //
          stringHours = inputString.substring(anIndex,iComma) ;
          stringMinutes = inputString.substring(iComma+1) ;
          //
          int iHours, iMinutes ;
          //
          iHours = stringHours.toInt() ;
          iMinutes = stringMinutes.toInt() ;
          //
          // Validate settings
          //
          if((iHours <= 23) && (iMinutes <= 59))
          {
            if(isAlarm == 0)
            {
              setTime(iHours,iMinutes) ;
            }
            else
            {
              setAlarm(iHours,iMinutes) ;
            }
            iError = 0 ;    // No errors
          }
        }
      }
    }
    else if (inputString.startsWith("gettime"))
    {
      lpszSyntax = "GetTime" ;
      getTime() ;
      iError=0 ;
    }
    else if (inputString.startsWith("getalarm"))
    {
      lpszSyntax = "GetAlarm" ;
      getAlarm() ;
      iError = 0 ;
    }
    else if (inputString.startsWith("setbright"))
    {
      lpszSyntax = "SetBright xx" ;
      setBright() ;
      iError = 0 ;
    }
    else if (inputString.startsWith("getbright"))
    {
      lpszSyntax = "GetBright" ;
      getBright() ;
      iError = 0 ;
    }
    else
    {
      Serial.println("Don't know you!") ;
    }
    //
    if(iError == -1)
    {
      errorInCommand(inputString,lpszSyntax) ;
    }
    // clear the string:
    inputString = "";
    stringComplete = false;
 }
}
///////////////////////////////////////////////////
// Command Processing
///////////////////////////////////////////////////

void setTime(int iHours, int iMinutes)
{
  DateTime was = RTC.now() ;
  DateTime now(was.year(),was.month(),was.day(),iHours,iMinutes, 0) ;
  //
  RTC.adjust(now) ;
  getTime() ;
  Serial.println("SetTime complete.");
}

void getTime(void)
{
  DateTime  now = RTC.now() ;
  char  szTimeBuffer[64] ;
  now.toString(szTimeBuffer,64) ;
  Serial.println(szTimeBuffer) ;
}

void setAlarm(int iHours, int iMinutes)
{
  iAlarmHour = iHours ;
  iAlarmMinute = iMinutes ;
  //
  // Save Alarm to EEPROM
  //
  EEPROM.update(eeAlarmHour,iAlarmHour) ;
  EEPROM.update(eeAlarmMinute,iAlarmMinute) ;
  Serial.print("Alarm set to ") ;
  Serial.print(iAlarmHour) ;
  Serial.print(":") ;
  Serial.println(iAlarmMinute) ;
}

void getAlarm(void)
{
  Serial.print("Alarm == ") ;
  Serial.print(iAlarmHour) ;
  Serial.print(":") ;
  Serial.println(iAlarmMinute) ;
}

void setBright(void)
{
  uint8_t iIndex = inputString.indexOf(" ") ;
  String strBrightness = inputString.substring(iIndex) ;
  ui8Bright = strBrightness.toInt() ;
  //
  // Update the EEPROM
  //
  EEPROM.update(eeBright,ui8Bright) ;
  strip.setBrightness(ui8Bright) ;
  strip.show();
  Serial.print("Brightness is now ") ;
  Serial.println(ui8Bright) ;
}

void getBright(void)
{
  Serial.print("Brightness is  ") ;
  Serial.println(ui8Bright) ;
}
//
//  Periodic check for alarm trigger
//
// The following method is called periodically by a timer
//

void timerCallback(void)
{
  //
  //  Read the curent date/time from the RTC
  //
  DateTime  now = RTC.now() ;
  int       iNowHour = now.hour() ;
  int       iNowMinute = now.minute() ;
  int       iDiffHour ;
  int       iDiffMinute ;
  //
  // Calculate the difference between the Alarm time and now
  //
  iDiffMinute = iAlarmMinute - iNowMinute ;
  //
  if(iDiffMinute < 0)
  {
    iDiffMinute = (60 - iNowMinute) + iAlarmMinute ;
    iNowHour += 1 ;
  }
  iDiffHour = iAlarmHour - iNowHour ;
  //
  if(iDiffHour < 0)
  {
    iDiffHour = (24 - iNowHour) + iAlarmHour ;
  }
  //
  // Difference must be less than one hour because we approach alarm
  // in 4 minute intervals
  //
  if(iDiffHour == 0)
  {
    //
    // Difference minutes must be 32 or less
    //
    if(iDiffMinute <= 32)
    {
      uint32_t pixelColor  ;
      //
      // Process the LEDs
      //
      ucLedOnCount = ledsOn[iDiffMinute] ;
      //
      // All off first
      //
      for(unsigned char ucIndex = 0 ; ucIndex < ucLedOnCount ; ucIndex++)
      {
          strip.setPixelColor(ucIndex,strip.Color(0, 0, 0)) ;
      }
      strip.show() ;
      //
      // Now the display
      //
      for(unsigned char ucIndex = 0 ; ucIndex < ucLedOnCount ; ucIndex++)
      {
        if(ucIndex < 4)
        {
          pixelColor = colorFirstSegment ;
        }
        else if(ucIndex < 6)
        {
          pixelColor = colorSecondSegment ;
        }
        else
        {
          pixelColor = colorThirdSegment ;
        }
        
        strip.setPixelColor(ucIndex,pixelColor) ;
      }
      strip.show() ;
      //
      // If this is the alarm point start the post alarm timer
      //
      if(ucLedOnCount == 8)
      {
        lPostAlarmActive = lPostAlarmCount ;
      }
    }
  }
  //
  // Is the post alarm timer active?
  //
  if(lPostAlarmActive != 0)
  {
    //
    // End of the hold time?
    //
    if(lPostAlarmActive == 1)
    {
      lPostAlarmActive = 0 ;
      //
      // All pixels off
      //
      for(unsigned char ucIndex = 0 ; ucIndex < 8 ; ucIndex++)
      {
          strip.setPixelColor(ucIndex,strip.Color(0, 0, 0)) ;
      }
      strip.show() ;
    }
    else
    {
      lPostAlarmActive-- ;
    }
  }
}

///////////////////////////////////////////////////////
// Utility Methods
///////////////////////////////////////////////////////

void errorInCommand(String & inputStr, char * lpszMessage)
{
    Serial.print("Error in command -> ") ;
    Serial.print(inputStr) ;
    Serial.print("Syntax is ->") ;
    Serial.println(lpszMessage) ;
}
/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    } 
  }
}

