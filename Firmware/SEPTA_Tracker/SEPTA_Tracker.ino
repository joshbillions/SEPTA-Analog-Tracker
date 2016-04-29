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
#define kGauge1   16
#define kGauge2   12
#define kGauge3   13
#define kGauge4   4
#define kGauge5   0
#define kGauge6   5

// Gauge Marks
#define kPosition0    0
#define kPosition1    205
#define kPosition2    410
#define kPosition3    615
#define kPosition4    820
#define kPosition5    1023

int positionArray[] = {kPosition0, kPosition1, kPosition2, kPosition3, kPosition4, kPosition5};

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

  pinMode(kGauge1, OUTPUT);
  pinMode(kGauge2, OUTPUT);
  pinMode(kGauge3, OUTPUT);
  pinMode(kGauge4, OUTPUT);
  pinMode(kGauge5, OUTPUT);
  pinMode(kGauge6, OUTPUT);

  for(int i = 0; i < 1023; i++){
    setAllGaugesPWM(i);
    Serial.println(i);
    delay(50);
  }

}


#pragma MARK - Indicators

void updateIndicators() {
  //digitalWrite(kBadPin, LOW);
  //digitalWrite(kGoodPin, LOW);
  if (minutesUntilNextArrival >= kAcceptableWaitTime || minutesUntilNextArrival <= kTooSoonTime ) {
    Serial.println("Don't leave yet");
    //digitalWrite(kBadPin, HIGH);
  } else {
    if (minutesUntilNextArrival <= kTooSoonTime + 2) {
      Serial.println("Leave Now!");
      //digitalWrite(kGoodPin, HIGH);
    } else {
      //digitalWrite(kGoodPin, HIGH);
    }
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
    //digitalWrite(kControlTestPin, led_status);
    led_status ^= 0x01;
    delay(100);
  }

  // Turn LED on when we are connected
  //digitalWrite(kControlTestPin, HIGH);
  Serial.println("WiFi Connected");
}


void loop() {
  runGaugeTests();
  
  //Look for switch settings
  //switchUp = digitalRead(kSwitchPin);

  //Clear Server Response Buffer
  clearServerResponseBuffer();

  // Connect to WiFi
  connectWiFi();

  // Attempt to connect to website
  if ( !getPage() ) {
    Serial.println("GET request failed");
    needsImmediateRefresh = true;

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
    //digitalWrite(kControlTestPin, LOW);
  }

  if (!needsImmediateRefresh) {
    delay(30000);
  } else {
    delay(2000); //Not soooo immediate...
    needsImmediateRefresh = false;
  }

}


