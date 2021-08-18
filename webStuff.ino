void initialiseWebServer() { // 159
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("sending index.html");
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // Routes to controls pumps
  server.on("/onPumpA", HTTP_GET, [](AsyncWebServerRequest * request) {

    if (!btnPressureSwitch.isPressed()) {
      digitalWrite(pinPumpA, HIGH); // APH 163 was LOW
      pumpARunning = true;
      pumpAState = "ON";

      digitalWrite(pinPumpB, LOW); // APH 163 was HIGH
      pumpBRunning = false;
      pumpBState = "OFF";

      manualON = true;
      manualControl = true;

      Serial.println("Pump A On");
    }
    else {
      Serial.println("No action - pressure switch pressed");
    }

    request->redirect("/");

  });

  server.on("/offPumpA", HTTP_GET, [](AsyncWebServerRequest * request) {
    digitalWrite(pinPumpA, LOW); // APH 163 was HIGH
    pumpARunning = false;
    pumpAState = "OFF";
    manualON = false;
    manualControl = false;
    request->send(SPIFFS, "/index.html", String(), false, processor);
    Serial.println("Pump A Off");

  });

  server.on("/onPumpB", HTTP_GET, [](AsyncWebServerRequest * request) {

    if (!btnPressureSwitch.isPressed()) {
      digitalWrite(pinPumpB, HIGH); // APH 163 was LOW
      pumpBRunning = true;
      pumpBState = "ON";

      digitalWrite(pinPumpA, LOW); // APH 163 was HIGH
      pumpARunning = false;
      pumpAState = "OFF";

      manualON = true;
      manualControl = true;

      Serial.println("Pump B On");
    }
    else {
      Serial.println("No action - pressure switch pressed");
    }

    request->redirect("/");
  });

  // Route to set GPIO
  server.on("/offPumpB", HTTP_GET, [](AsyncWebServerRequest * request) {
    digitalWrite(pinPumpB, LOW); // APH 163 was HIGH
    pumpBRunning = false;
    pumpBState = "OFF";
    manualON = false;
    manualControl = false;
    request->send(SPIFFS, "/index.html", String(), false, processor);
    Serial.println("Pump B Off");

    //showHome();
  });

  // APH157 This sends data to webpage
  server.on("/ampsA", HTTP_GET, [](AsyncWebServerRequest * request) {
    sprintf(strMsg, "%3.2fA (limit %3.2fA)", pumpAAmps, pumpAThreshold); // APH added 'A ' (limit
    request->send_P(200, "text/plain", strMsg);
  });

  server.on("/stateA", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", pumpAState.c_str());
  });

  server.on("/ampsB", HTTP_GET, [](AsyncWebServerRequest * request) {
    sprintf(strMsg, "%3.2fA (limit %3.2fA)", pumpBAmps, pumpBThreshold); // APH added 'A ' (limit
    request->send_P(200, "text/plain", strMsg);
  });

  server.on("/stateB", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", pumpBState.c_str());
  });

  server.on("/setA", HTTP_GET, [](AsyncWebServerRequest * request) {
    // sprintf(strMsg, "Pump A - AMPS = %3.2fA THRESHOLD = %3.2fA)", pumpAAmps, pumpAThreshold); Serial.println(strMsg);
    // APH157 Changed
    if (pumpAmps > 0.05) {
      pumpAThreshold = pumpAAmps / 1.125; // V139 // 1.35; // 1.7; V137 // was 1.8 & originally = 2
      if (pumpAThreshold < 0.2) pumpAThreshold = defaultThreshold; // APH was 0.2 & 2.95
      saveEEData();
      // APH157 Added Changed ZZZZZZ

      // turn it off
      digitalWrite(pinPumpA, LOW); // APH 163 was HIGH
      pumpARunning = false;
      pumpAState = "OFF";
      manualON = false;
      manualControl = false;
      sprintf(strMsg, "Pump A threshold set to %3.2f amps", pumpAThreshold); Serial.println(strMsg);
    }

    // APH157 Changed
    //request->send(SPIFFS, "/index.html", String(), false, processor);
    request->redirect("/");
    Serial.println("Set A");
  });

  server.on("/setB", HTTP_GET, [](AsyncWebServerRequest * request) {
    // APH157 Added Changed
    // sprintf(strMsg, "Pump B - AMPS = %3.2fA THRESHOLD = %3.2fA)", pumpBAmps, pumpBThreshold); Serial.println(strMsg);
    if (pumpAmps > 0.05) {
      pumpBThreshold = pumpBAmps / 1.125; // V139 // 1.35; // 1.7; V137 // was 1.8 & originally = 2
      if (pumpBThreshold < 0.2) pumpBThreshold = defaultThreshold; // APH was 0.2 & 2.95
      // APH157 Added Changed
      saveEEData();

      // turn it off
      digitalWrite(pinPumpB, LOW); // APH 163 was HIGH
      pumpBRunning = false;
      pumpBState = "OFF";
      manualON = false;
      manualControl = false;
      sprintf(strMsg, "Pump B threshold set to %3.2f amps", pumpBThreshold); Serial.println(strMsg);
    }

    request->redirect("/"); // 157
    Serial.println("Set B");
  });

  server.on("/setB", HTTP_GET, [](AsyncWebServerRequest * request) {

    // APH157 Added Changed
    // if (pumpBThreshold < 0.2) pumpBThreshold = 2.25; // APH was 0.2 & 2.95
    if (pumpBAmps != 0) { // Only change threshold if page refreeshed and pump IS running
      pumpBThreshold = pumpBAmps / 1.125; // V139 // 1.35; // 1.7; // was 2 V137
      if (pumpBThreshold < 0.2) pumpBThreshold = 2.25; // APH was 0.2 & 2.95
    }
    // APH157 Added Changed
    saveEEData();

    // turn it off
    digitalWrite(pinPumpB, LOW); // APH 163 was HIGH
    pumpBRunning = false;
    pumpBState = "OFF";
    manualON = false;
    manualControl = false;
    sprintf(strMsg, "Pump B threshold set to %3.2f amps", pumpBThreshold); Serial.println(strMsg);

    request->send(SPIFFS, "/index.html", String(), false, processor);

    Serial.println("Set B");
  });

  // APH Added new button
  server.on("/swapPumps", HTTP_GET, [](AsyncWebServerRequest * request) { // 136
    swapNo += 1;
    Serial.print(swapNo);

    if (activePump == 'A') {
      activePump = 'B';
      Serial.println(" - Pump B now active");
    }
    else {
      activePump = 'A';
      Serial.println(" - Pump A now active");
    }

    EEPROM.write(EE_ACTIVE_PUMP, activePump);
    EEPROM.commit(); // APH 154 added

    if (pumpARunning) {
      digitalWrite(pinPumpA, LOW); // APH 163 was HIGH
      pumpARunning = false;
      pumpAState = "OFF";
      Serial.println("Pump A Stopped");
      digitalWrite(pinPumpB, HIGH); // APH 163 was LOW
      pumpBRunning = true;
      pumpBState = "ON";
      Serial.println("Started Pump B");

    }
    else if (pumpBRunning) {
      digitalWrite(pinPumpB, LOW); // APH 163 was HIGH
      pumpBRunning = false;
      pumpBState = "OFF";
      Serial.println("Pump B Stopped");
      digitalWrite(pinPumpA, HIGH); // APH 163 was LOW
      pumpARunning = true;
      pumpAState = "ON";
      Serial.println("Started Pump A");
    }

    if (pumpARunning || pumpBRunning) {
      transitionTick.reset();
      inTransition = true;
    }

    changeDisplay = true; // 149
    changeDisplayTick.reset();

    request->send(SPIFFS, "/index.html", String(), false, processor);

  });

  server.on("/activePump", HTTP_GET, [](AsyncWebServerRequest * request) {
    sprintf(strMsg, "%c) ", activePump); //Serial.println(strMsg);
    request->send_P(200, "text/plain", strMsg); // APH trying this again 135
  });

  server.on("/time", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getTime().c_str());
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>

    // APH 156 Added check
    if (request->hasParam(PARAM_INPUT_1)) { // 1 = "timenow"
      inputMessage = request->getParam(PARAM_INPUT_1)->value();

      int n = inputMessage.length();
      char char_array[n + 1];

      strcpy(char_array, inputMessage.c_str());

      char str[3];
      for (int i = 0; i < 3; i++) str[i] = 0;

      int newhh, newmm;

      str[0] = char_array[0];
      str[1] = char_array[1];
      newhh = atoi(str);

      str[0] = char_array[3]; // APH was str[0] = char_array[2];
      str[1] = char_array[4]; // APH was str[1] = char_array[3];
      newmm = atoi(str);

      if (newhh == 0 && newmm == 0) {// APH 156 Added check
        Serial.println("Time not set ERROR");
      }
      else {
        hh = newhh;
        mm = newmm;
        ss = 0;
        sprintf(strMsg, "Time set to %d:%d", hh, mm); Serial.println(strMsg);
      }

      rtc.adjust(DateTime(DateTime(2021, 8, 16, hh, mm, 0))); //sets the RTC with an explicit date & new time

      delay(1000);
      request->send(SPIFFS, "/index.html", String(), false, processor);
    }

    else if (request->hasParam(PARAM_INPUT_2)) { // 2 = "ontime"
      inputMessage = request->getParam(PARAM_INPUT_2)->value();

      int n = inputMessage.length();
      char char_array[n + 1];

      strcpy(char_array, inputMessage.c_str());

      char str[3];
      for (int i = 0; i < 3; i++) str[i] = 0;

      str[0] = char_array[0];
      str[1] = char_array[1];
      nightEndHH = atoi(str);

      str[0] = char_array[3]; // APH was 2
      str[1] = char_array[4]; // APH was 3
      nightEndMM = atoi(str);

      sprintf(strMsg, "Night end  = %02d:%02d", nightEndHH, nightEndMM); Serial.println(strMsg);

      saveEEData();

      request->send(SPIFFS, "/index.html", String(), false, processor);
    }

    else if (request->hasParam(PARAM_INPUT_3)) { // 3 = "offtime"
      inputMessage = request->getParam(PARAM_INPUT_3)->value();

      int n = inputMessage.length();
      char char_array[n + 1];

      strcpy(char_array, inputMessage.c_str());

      char str[3];
      for (int i = 0; i < 3; i++) str[i] = 0;

      str[0] = char_array[0];
      str[1] = char_array[1];
      nightStartHH = atoi(str);

      str[0] = char_array[3]; // APH was 2
      str[1] = char_array[4]; // APH was 3
      nightStartMM = atoi(str);

      sprintf(strMsg, "Night start = %02d:%02d", nightStartHH, nightStartMM); Serial.println(strMsg);

      saveEEData();

      request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else {
      //inputMessage = "No message sent";
      //inputParam = "none";
    }
  });

  // special upload page
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest * request) {
    makeHeader();
    fileHeader += "<h2>Upload</h2>" + ptr;

    request->send(200, "text/html", fileHeader);
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200);
  }, [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t* data,
        size_t len, bool final) {
    handleUpload(request, filename, index, data, len, final);
  });

  makeHTML();
}


// Replaces placeholder with state value
// APH157 Added Changed
String processor(const String& var) {
  //Serial.println(var);

  if (var == "STATEA") {
    if (!digitalRead(pinPumpA)) ledState = "ON"; //
    else ledState = "OFF";
    return ledState;
  }

  else if (var == "STATEB") {
    if (!digitalRead(pinPumpB)) ledState = "ON"; //
    else ledState = "OFF";
    return ledState;
  }

  else if (var == "ampsA") {
    sprintf(strMsg, "%3.2fA (limit %3.2fA)", pumpAAmps, pumpAThreshold);
    return String(strMsg);
  }

  else if (var == "ampsB") {
    sprintf(strMsg, "%3.2fA (limit %3.2fA)", pumpBAmps, pumpBThreshold);
    return String(strMsg);
  }

  else if (var == "time") {
    Serial.println(getTime()); // APH enabled again for testing
    return getTime();
  }

  else if (var == "systemOnTime") { // I have changed from currentontime
    char str[5];
    sprintf(str, "%02d:%02d", nightEndHH, nightEndMM); // APH added ':' was sprintf(str, "%02d%02d", nightEndHH, nightEndMM);
    return String(str);
  }

  else if (var == "systemOffTime") { // I have changed from currentofftime
    char str[5];
    sprintf(str, "%02d:%02d", nightStartHH, nightStartMM); // APH added ':'
    return String(str);
  }

  return String();
}

void handleUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {

  if (!index) {
    if (!filename.startsWith("/")) filename = "/" + filename;

    Serial.print("Uploading : "); Serial.println(filename);

    fsUploadFile = SPIFFS.open(filename, "w");
  }

  if (len) {
    if (fsUploadFile) fsUploadFile.write(data, len);
  }

  if (final) {
    if (fsUploadFile) {
      fsUploadFile.close();
      Serial.println((String)"Uploaded: " + filename + ", " + index + ", " + len);
      request->_tempFile.close();
      request->send(200, "text/plain", "File Uploaded !");
    }
    else {
      request->send(200, "text/plain", "500: couldn't create file");
    }
  }
}

void makeHeader() {
  fileHeader = "<!DOCTYPE HTML><html><head><title>PC2021</title><meta name = 'viewport' content = 'width=device-width, initial-scale=1'>";
  fileHeader += "<link rel = 'stylesheet' type = 'text/css' href = 'style.css'>";
  fileHeader += "</head><body><div class = 'topnav'><h1>PC2021</h1></div>";
}

void makeHTML() {
  ptr += "<form method=\"POST\" enctype=\"multipart/form-data\" action  = \"/upload\">";
  ptr += "   <input type=\"file\" name=\"filename\" action=\"/upload\">";
  ptr += "   <input class=\"button\" type=\"submit\" value=\"Upload\">";
  ptr += "</form>";

  ptr += "</body>\n";
  ptr += "</html>\n";
}
