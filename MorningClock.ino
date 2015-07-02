#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <RTC_DS3231.h>


//
// LED Morning Clock
//
// Copyright Sid Price 2015 - All rights reserved
//

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

RTC_DS3231 RTC ;

void setup() {
  // initialize serial:
  Serial.begin(57600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
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
  // This section grabs the current datetime
  //
  DateTime now = RTC.now();
  //
  // Print the current time to the serial port
  //
  char  szTimeBuffer[64] ;
  now.toString(szTimeBuffer,64) ;
  Serial.println(szTimeBuffer) ;
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
//  2. Reads the RTC
//    a. Checks if the TOD is within 30 minutes of the Alarm and calls appropriate processing
//

void loop() {

 //
  //  Process any strings that arrive on the serial line
  //
  if (stringComplete) {
    //Serial.println(inputString); 
    //
    if(inputString.startsWith("sid"))
    {
      Serial.println("Hi Sidney") ;
    }
    else
    {
      Serial.println("Don't know you!") ;
    }
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
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

