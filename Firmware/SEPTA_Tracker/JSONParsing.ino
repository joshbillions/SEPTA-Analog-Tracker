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

