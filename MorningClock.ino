#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <RTC_DS3231.h>

#include <SimpleTimer.h>


//
// LED Morning Clock
//
// Copyright Sid Price 2015 - All rights reserved
//

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

void setup() {
  // initialize serial:
  Serial.begin(57600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(64);
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
  Serial.println(iTimerID) ;
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
  timer.run() ;
  //
  //  Process any strings that arrive on the serial line
  //
  if (stringComplete)
  {
    int iError = -1 ;
    char  * lpszSyntax = "" ;
    //
    if(inputString.startsWith("SetTime"))
    {
      lpszSyntax = "SetTime hh:mm" ;
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
            setTime(iHours,iMinutes) ;
            iError = 0 ;    // No errors
          }
        }
      }
    }
    else if (inputString.startsWith("GetTime"))
    {
      lpszSyntax = "GetTime" ;
      getTime() ;
      iError=0 ;
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

//
//  Periodic check for alarm trigger
//
// The following method is called periodically by a timer
//

void timerCallback(void)
{
  Serial.println("Timer called") ;
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

