#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>


// WiFi information
const char WIFI_SSID[] = "SSID";
const char WIFI_PSK[] = "PASSWORD";

// Remote site information
const char http_site[] = "www3.septa.org";
const int http_port = 80;

// Pin definitions
#define kControlTestPin   5
#define kGoodPin          0
#define kBadPin           4
#define kSwitchPin        13
#define kButtonPin        12

// Global variables
WiFiClient client;

//Septa Stop IDs
int stopIDs[2] = {92, 21068};
bool switchUp = false;

//Server response buffer
#define kResponseBufferSize 2048
char serverResponseBuffer[kResponseBufferSize];
uint16_t serverResponseBufferSize = 0;

//Time tracking
int currentHour;
int currentMinute;

int targetHour;
int targetMinute;

int minutesUntilNextArrival;

#define kAcceptableWaitTime 6
#define kTooSoonTime 2

bool needsImmediateRefresh = false;

void setup() {
  // Set up serial console to read web page
  Serial.begin(115200);
  Serial.println("\n\n\nSEPTA Tracker...READY");

  // Set up LED for debugging
  pinMode(kControlTestPin, OUTPUT);
  pinMode(kGoodPin, OUTPUT);
  pinMode(kBadPin, OUTPUT);
  pinMode(kSwitchPin, INPUT_PULLUP);
  pinMode(kButtonPin, INPUT_PULLUP);

  turnOffIndicators();
}

void loop() {
  //Look for switch settings
  switchUp = digitalRead(kSwitchPin);

  //Clear Server Response Buffer
  clearServerResponseBuffer();

  // Connect to WiFi
  connectWiFi();

  // Attempt to connect to website
  if ( !getPage() ) {
    Serial.println("GET request failed");
    needsImmediateRefresh = true;
    turnOffIndicators();
  } else {
    delay(1000);
  }

  // If there are incoming bytes, print them
  if ( client.available() ) {
    parseResponse();
    printCurrentAndTargetTimes();
    updateIndicators();
  }

  // If the server has disconnected, stop the client and WiFi
  if ( !client.connected() ) {
    Serial.println();

    // Close socket and wait for disconnect from WiFi
    client.stop();
    if ( WiFi.status() != WL_DISCONNECTED ) {
      WiFi.disconnect();
    }

    // Turn off LED
    digitalWrite(kControlTestPin, LOW);
  }

  if (!needsImmediateRefresh) {
    delay(30000);
  } else {
    delay(2000); //Not soooo immediate...
    needsImmediateRefresh = false;
  }

}

#pragma MARK - Indicators

void turnOffIndicators(){
  digitalWrite(kGoodPin, LOW);
  digitalWrite(kBadPin, LOW);
}

void updateIndicators() {
  digitalWrite(kBadPin, LOW);
  digitalWrite(kGoodPin, LOW);
  if (minutesUntilNextArrival >= kAcceptableWaitTime || minutesUntilNextArrival <= kTooSoonTime ) {
    Serial.println("Don't leave yet");
    digitalWrite(kBadPin, HIGH);
  } else {
    if (minutesUntilNextArrival <= kTooSoonTime + 2) {
      Serial.println("Leave Now!");
      digitalWrite(kGoodPin, HIGH);
    } else {
      digitalWrite(kGoodPin, HIGH);
    }
  }

}

#pragma MARK - Server Response Parsing

void clearServerResponseBuffer() {
  memset(serverResponseBuffer, 0, sizeof(char) * kResponseBufferSize);
  serverResponseBufferSize = 0;
}

void parseResponse() {
  int openBrackets = 0;
  int closeBrackets = 0;
  while (client.available() > 0) {
    char c = client.read();
    //Keep track of open and close brackets to find JSON. Dirty, but it works.
    if ( c == '{' ) {
      openBrackets++;
    }

    if ( c == '}' ) {
      closeBrackets++;
    }

    if (openBrackets > 0 ) {
      //If we've found an open bracket start recording the response to the buffer until
      //the number of open brackets matches the closed brackets
      serverResponseBuffer[serverResponseBufferSize] = c;
      serverResponseBufferSize++;
      if (openBrackets == closeBrackets) {
        parseJSON();
      }
    } else {
      //Grab the current time from the HTTP Header Fields as well. A RTC would be nice,
      //but this keeps things cheap
      if ( c == 'D' ) {
        //Cheat and skip out of the whole loop.
        if (client.read() == 'a') {
          if (client.read() == 't') {
            if (client.read() == 'e') {
              if (client.read() == ':');
              String rawDate = client.readStringUntil('\r');
              parseHTTPRawDate(rawDate);
            }
          }
        }
      }
    }
  }
}



void parseJSON() {
  StaticJsonBuffer<kResponseBufferSize> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(serverResponseBuffer);
  if (switchUp) { //Bus
    const char *nextArrival = root["48"]["0"]["date"];
    //Sometimes the SEPTA API will start numbering new arrivals at 1 instead of 0, not sure why.
    if (nextArrival) {
      parseJSONDate(String(nextArrival));
    } else {
      const char *secondNextArrival = root["48"]["1"]["date"];
      if (secondNextArrival) {
        parseJSONDate(String(secondNextArrival));
      } else {
        Serial.println("JSON Parsing Error:");
        Serial.println(serverResponseBuffer);
        needsImmediateRefresh = true;
      }
    }
  }else{ //Trolly
     const char *nextArrival = root["15"]["0"]["date"];
    //Sometimes the SEPTA API will start numbering new arrivals at 1 instead of 0, not sure why.
    if (nextArrival) {
      parseJSONDate(String(nextArrival));
    } else {
      const char *secondNextArrival = root["15"]["1"]["date"];
      if (secondNextArrival) {
        parseJSONDate(String(secondNextArrival));
      } else {
        Serial.println("JSON Parsing Error:");
        Serial.println(serverResponseBuffer);
        needsImmediateRefresh = true;
      }
    }
  }
}

#pragma MARK - Date Parsing

void printCurrentAndTargetTimes() {
  Serial.print("Current Time: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.println(currentMinute);

  Serial.print("Arrival Time: ");
  Serial.print(targetHour);
  Serial.print(":");
  Serial.println(targetMinute);

  Serial.print("Minutes until next arrival: ");
  Serial.println(minutesUntilNextArrival);

  if ((currentHour == 0 && currentMinute == 0) || (targetHour == 0 && targetMinute == 0)) {
    Serial.println("Time Parsing Error:");
    Serial.println(serverResponseBuffer);
    needsImmediateRefresh = true;
  }
}

void parseJSONDate(String jsonDate) {
  //Check AM/PM, not sure why SEPTA would think that's a good idea.
  bool dateIsPM = false;
  if (jsonDate.charAt(jsonDate.length() - 1) == 'p') {
    dateIsPM = true;
  }

  int timeSeperator = jsonDate.indexOf(':');

  int jsonHours = jsonDate.substring(0, timeSeperator).toInt();
  if ( dateIsPM ) {
    jsonHours += 12;
  }else if (jsonHours == 12){
    jsonHours = 0;
  }

  targetHour = jsonHours;
  targetMinute = jsonDate.substring(timeSeperator + 1, timeSeperator + 3).toInt();

  int targetAdjustment = 0;

  if (targetHour != currentHour) {
    targetAdjustment = 60;
  }

  minutesUntilNextArrival = (targetMinute + targetAdjustment) - (currentMinute);

}

void parseHTTPRawDate(String rawDate) {
  //This would be way more efficient using char arrays instead of String manipulation
  //but we're on an ARM, so whatever?

  //Find the time seperators
  int firstSeperator = rawDate.indexOf(':');
  int lastSeperator = rawDate.lastIndexOf(':');
  //Widen the range to grab hours, we don't need to worry about seconds
  firstSeperator -= 2;
  int gmtHour = rawDate.substring(firstSeperator, firstSeperator + 2).toInt();
  currentMinute = rawDate.substring(firstSeperator + 3, lastSeperator).toInt();

  //Convert GMT Hour to EST Hour (+5 Hour Difference)
  if (gmtHour > 5) {
    currentHour = gmtHour - 5;
  } else {
    int difference = 4 - gmtHour;
    currentHour = 23 - difference;
  }
}

// Attempt to connect to WiFi
void connectWiFi() {

  byte led_status = 0;

  // Set WiFi mode to station (client)
  WiFi.mode(WIFI_STA);

  // Initiate connection with SSID and PSK
  WiFi.begin(WIFI_SSID, WIFI_PSK);

  // Blink LED while we wait for WiFi connection
  while ( WiFi.status() != WL_CONNECTED ) {
    digitalWrite(kControlTestPin, led_status);
    led_status ^= 0x01;
    delay(100);
  }

  // Turn LED on when we are connected
  digitalWrite(kControlTestPin, HIGH);
  Serial.println("WiFi Connected");
}

// Perform an HTTP GET request to a remote page
bool getPage() {

  // Attempt to make a connection to the remote server
  if ( !client.connect(http_site, http_port) ) {
    Serial.println("Unable to connect to remote server");
    return false;
  }

  // Make an HTTP GET request
  if (switchUp) {
    Serial.println("Making Bus Request");
    client.println("GET /hackathon/BusSchedules/?req1=92 HTTP/1.1");
  } else {
    Serial.println("Making Trolly Request");
    client.println("GET /hackathon/BusSchedules/?req1=21068 HTTP/1.1");
  }
  client.print("Host: ");
  client.println(http_site);
  client.println("Connection: close");
  client.println();

  return true;
}
