/*
   Name:       PC2021.ino
   Created: 24/11/2019 14:58:05
   Author:     DAVID-HP\David
*/
#define Ver 41 // fir the digole!!!!
#define _Digole_Serial_I2C_ // for the graphics

// APH Beep(3,1,9,1000) means beep 3 times for 0.1 sec with an interval of 0.9secs between at a frequency of 1Kz

/*151
  109 - pump on start
  110 - beeping
  111 - night back
  112 - don't turn pump on if pressurised
  113 - night thing
  114 - two second changeover/1 beep/
  115 - loggin
  116 - time
  117 - uses DS1307
  118 - no beep/small changes
  119 - off delay
  120 - don't change if pressurising
  121 - minimum thresholds
  122 - threshold set to 50%
  123 - cals at 50% of water load.
  124 - adds night start inhibit
  125 - different night detection algoritihm
  126 - changeover 4 secs
  127 - pressurise after night
  128 - don't pressurise in start phase
  129 - adds mins since start
  130 - fix night detect bug
  131 - all good again
  132 - saves active pump to EEPROM
  133 - adds swap pump button
  134 - bug fixes to 132 and 133
  135 - change how swap active pump but not run pump
  136 - swap x but start only if something is running
  137 - Threshold factor changed from 1.7 to 1.35 & default to 2.5
  138 - adds catch all 3 minute timeout
  139 - tweaked threshold formular APH
  140 - Added LCD & Buzzer extension box
  141 - show X if pumps have changed
  142 - sound both buzzers
  143 - add pending refill
  144 - use buttons for PS and pump
  145 - tweaked extension display APH
  146 - tidy up
  147 - use images for pumps
  148 - add extension mode
  149 - decouples home screen update from web server request
  150 - don't swap pumps if refill pending when button pressed
  151 - APH added my text images
  152 - enable/disable WiFi
  153 - APH added WiFi to show when enabled
  154 - APH changed displayed text when barrel empty, add define for 'testing' & 'david'
  155 - APH Added check when booting up & try last pump. Added commit for Pump EE. Change display when empty.
  156 - APH Added check if set time value zero, if so dont set time.
  157 - APH Renamed project PC2021 & Added option for both dev & 2021 boards.
*/
int version = 157;

//#define david // APH 154 added this feature from Energy Miser
#define testing // APH 155 The is used to boot with WiFi on for testing wothout extension box

#define useRTC // These are for normal use
#define useWebServer // These are for normal use
//#define USE_BUTTONS_FOR_PS_AND_PUMP // These are for testing using PCB buttons
//#define useWifi // Dont use this, its David please delete if you agree


#include <WiFi.h>
#include <Wire.h>
#include <RTClib.h>
#include "ESPAsyncWebServer.h"
#include "FS.h"
#include <AsyncTCP.h>
#include "SPIFFS.h"
#include <Metro.h>
#include <Button.h>
#include <EEPROM.h>
//#include <DS1302.h>
#include <DigoleSerial.h>
#include "imageFront.h" // 145
#include "imageRear.h"
#include "imagePump.h"
#include "imageRunning.h"
#include "imageEmpty.h" // APH 155 added
// test#include "empty3.h"
#include "pitches.h" // APH 155 added

// colors
#define RED 0xe0
#define ORANGE 0xf0
#define GREEN 0x10
#define YELLOW 0xfc
#define WHITE 0xff
#define BLACK 0x00
#define BLUE 0x06
#define GREY 0x92
#define TURQUOISE 0x1f

const unsigned char fonts[] = { 6, 10, 18, 51, 120, 123 };

DigoleSerialDisp gDisp(&Wire, '\x27');

#ifdef useRTC
RTC_DS1307 rtc;
#endif

// APH 157 added this option to cover both boards
//#define DEV_BOARD
#ifdef DEV_BOARD
#define pinBuzzer 2 // pin 25 -> IO23 on PCB
#define pinNewBuzzer 4 // pin 26 -> IO26 on PCB
#define pinScreenButton 12 // pin 28 -> IO25 on PCB
#define pinSet 13 // pin 29 -> IO32 on PCB
#define pinB 14 // pin 30 -> IO17 on PCB
#define pinA 15 // pin 31 -> IO16 on PCB
#define pinPumpA 18 // pin 35 -> IO18 on PCB
#define pinPumpB 19 // pin 36 -> IO19 on PCB
#define pinPressureSwitch 39 // pin 10 was 39/2 ->IO5 on PCB
#define pinPumpCurrent 36 // pin 1 -> IO35 on PCB
#else
#define pinNewBuzzer 23
#define pinScreenButton 25
#define pinSet 32
#define pinPumpA 18
#define pinPumpB 19
#define pinB 17
#define pinA 16
#define pinPressureSwitch 5
#define pinPumpCurrent 35
#define pinAmplifier 26 // was pinBuzzer
#endif

// #define pinPSLED 25 // pin 10 APH 137 removed

Button btnSet = Button(pinSet, true, true, 100);
// Button btnScreen = Button(pinScreenButton, true, true, 100); // APH 154 changed

#ifdef USE_BUTTONS_FOR_PS_AND_PUMP
Button btnPressureSwitch = Button(pinA, true, true, 500);
Button btnTankOutOfWater = Button(pinB, true, true, 500);
Button btnScreen = Button(pinSet, true, true, 500); // APH 154 added
#else
Button btnPressureSwitch = Button(pinPressureSwitch, true, true, 500);
Button btnA = Button(pinA, true, true, 100);
Button btnB = Button(pinB, true, true, 100);
Button btnScreen = Button(pinScreenButton, true, true, 100); // APH 154 moved from above
#endif

#define EE_ACTIVE_PUMP 1 // 1
#define EE_A_THRESHOLD 3 // 2
#define EE_B_THRESHOLD 5 // 2
#define EE_NIGHT_START_HH 7 // 1
#define EE_NIGHT_START_MM 8 // 1
#define EE_NIGHT_END_HH 9 // 1
#define EE_NIGHT_END_MM 10 // 1

#define EEPROM_SIZE 20

String APssid = "My_LunarVan", APpassword = "aphlunar";
boolean APActive = false;

Metro secTick = Metro(1000);
Metro sampleTick = Metro(50);
Metro transitionTick = Metro(4000);
Metro pumpStartTick = Metro(3000);
Metro pressuriseTick = Metro(2000);
Metro tenthTick = Metro(100);
Metro clickTick = Metro(1);
Metro allOff = Metro(150000); // 138 was 180000
Metro changeDisplayTick = Metro(100); // 149
Metro longPress = Metro(1000); // 151

int longPressCount = 0;

boolean changeDisplay = false;

#define clickFreq 1000

// settings
typedef struct {
  char ssid[30];
  char pass[30];
} WifiNetwork;

struct WifiConfig {
  WifiNetwork net[10];
};
WifiConfig wifi_conf;

//int wifi_net_no = 0;
//WiFiMulti wifiMulti;

// time
int hh, mm, ss, minsSinceStart;
int nightStartHH, nightStartMM, nightEndHH, nightEndMM;
char strTime[10];

// pump stuff
boolean pumpARunning = false, pumpBRunning = false, tapOpen = false, inTransition = false, pumpIsStarting = false, manualControl = false;
boolean manualON = false, night = false, pressurising = false;
String pumpAState, pumpBState;

//double pumpVolts, pumpAmps;
double pumpAmps, pumpAAmps, pumpBAmps, pumpVolts, noPumpVolts, pumpAThreshold, pumpBThreshold, defaultThreshold; // APH157 Added defaultThreshold
#define MAX_SAMPLES 40
int pumpSamples[MAX_SAMPLES + 1], sampleIndex;
byte activePump = 'A';

#define NO_OF_BEEPS 2

// Constants
#ifdef david
const char* ssid = "WiFi_I_Am";  // Enter SSID David
const char* password = "malts-tap-CLAD";  //Enter Password David
#else
const char* ssid = "WiFi-is-Great";  // Enter SSID Andrew
const char* password = "milana99";  // Enter Password Andrew
#endif

const char* PARAM_INPUT_1 = "timenow";
const char* PARAM_INPUT_2 = "ontime";
const char* PARAM_INPUT_3 = "offtime";

String ledState;
const int ledPin = 19;

AsyncWebServer server(80);

char strMsg[300];

// music APH 154 now using pitches.h
//#define NOTE_C4 262
//#define NOTE_G3 196
//#define NOTE_A3 220
//#define NOTE_B3 247
//#define NOTE_B4 494

boolean beeping = false, displaybeeping = false, extensionMode = true;  // 148
unsigned long startMillis;

int onTime, onCount, offCount, offTime, beepCount, beepFreq;


int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// note durations: 4 = quarter note, 8 = eighth note
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

//// Array with the notes in the melody (see pitches.h for reference)
//int melody[] = {NOTE_A4, NOTE_A4, NOTE_A4, NOTE_F4, NOTE_C5, NOTE_A4, NOTE_F4, NOTE_C5, NOTE_A4, NOTE_E5, NOTE_E5, NOTE_E5, NOTE_F5, NOTE_C5, NOTE_A4, NOTE_F4, NOTE_C5, NOTE_A4};
//
//// Array with the note durations: a quarter note has a duration of 4, half note 2 etc.
//int noteDurations[] = {4, 4, 4, 5, 16, 4, 5, 16, 2, 4, 4, 4, 5, 16, 4, 5, 16, 2};


//// the melody sequence:
//int melody[] = {1, 3, 5, 1,
//                1, 3, 5, 1,
//                5, 6, 8, 5, 6, 8,
//                8, 10, 8, 6, 5, 1,
//                8, 10, 8, 6, 5, 1,
//                1, -4, 1,
//                1, -4, 1
//               };
//// the rhythm sequence:
//int rhythm[] = {4, 4, 4, 4,
//                4, 4, 4, 4,
//                4, 4, 2,
//                4, 4, 2,
//                8, 8, 8, 8, 4, 4,
//                8, 8, 8, 8, 4, 4,
//                4, 4, 2,
//                4, 4, 2
//               };

// mode
byte sysMode = 0;
#define modeNormal 0
#define modeSetTime 1
#define modeSetNightStart 2
#define modeSetNightEnd 3

// display
byte display = 0;
#define dispHome 0
#define dispPumpRunning 1

boolean setHours, setMins, nightStartInhibit = true;
#define NIGHT_INHIBIT_MINS 10 // APH 140 use 1 for testing
int minsON = 0;

// 143
boolean refillPending = false;
// 144 use button for tank out of water
boolean outOfWater = false;
// 148
boolean ignoreFirstRelease = true;
// 150
int swapNo = 0;

void setup() {

  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  gDisp.begin();
  gDisp.clearScreen();

  EEPROM.begin(EEPROM_SIZE);

  pinMode(ledPin, OUTPUT);
  // APH 157  pinMode(pinPSLED, OUTPUT);

  pinMode(pinPumpA, OUTPUT);
  pinMode(pinPumpB, OUTPUT);
  digitalWrite(pinPumpA, HIGH);
  digitalWrite(pinPumpB, HIGH);

  pinMode(pinPumpCurrent, INPUT);
  pinMode(pinPressureSwitch, INPUT);

  pinMode(pinSet, INPUT_PULLUP);
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  // APH Version 140
  pinMode(pinScreenButton, INPUT_PULLUP);
  pinMode(pinNewBuzzer, OUTPUT);
  // APH Version

  pumpAState = "OFF"; pumpBState = "OFF";
  // APH157 Added Changed
  defaultThreshold = 2.25;
  // APH157 Added Changed
  analogReadResolution(10);

  getEEData();
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // dir of SPIFFS
  //File root = SPIFFS.open("/");

  //File dfile = root.openNextFile();

  //while (dfile) {
  //  Serial.println(dfile.name());
  //  dfile = root.openNextFile();
  //}

#ifdef useRTC
  if (!rtc.isrunning()) {
    Serial.println("setting the time"); // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
  hh = now.hour(); mm = now.minute(); ss = now.second();
#endif

#ifdef useWifi
  Serial.println("Connecting Wifi...");

  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries += 1;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to "); Serial.print(WiFi.SSID());
    Serial.print(" with IP : ");  Serial.println(WiFi.localIP());
  }
#else

  WiFi.mode(WIFI_AP);
  WiFi.softAP(APssid.c_str(), APpassword.c_str());

  delay(100);
  //Serial.println(); Serial.print(APssid); Serial.println(" access point started");

  // APH added for Testing Only 154
#ifdef testing
  WiFi.disconnect(false); // WiFi ON at boot-up
  WiFi.enableAP(true);
#else
  WiFi.disconnect(true); // 152 WiFi OFF at boot-up
  WiFi.enableAP(false);
#endif
  //  WiFi.disconnect(true); // 152 WiFi OFF at boot-up
  //  WiFi.enableAP(false);

  //  WiFi.disconnect(false); // WiFi ON at boot-up
  //  WiFi.enableAP(true);
  // APH Testing Only

  //Serial.println();
  //Serial.print("IP address: ");
  //Serial.println(WiFi.softAPIP());

#endif

#ifdef useWebServer
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
      digitalWrite(pinPumpA, LOW);
      pumpARunning = true;
      pumpAState = "ON";

      digitalWrite(pinPumpB, HIGH);
      pumpBRunning = false;
      pumpBState = "OFF";

      manualON = true;
      manualControl = true;
      request->send(SPIFFS, "/index.html", String(), false, processor);
      Serial.println("Pump A On");
    }

    //showPumpScreen();
  });

  server.on("/offPumpA", HTTP_GET, [](AsyncWebServerRequest * request) {
    digitalWrite(pinPumpA, HIGH);
    pumpARunning = false;
    pumpAState = "OFF";
    manualON = false;
    manualControl = false;
    request->send(SPIFFS, "/index.html", String(), false, processor);
    Serial.println("Pump A Off");

    //showHome();
  });

  server.on("/onPumpB", HTTP_GET, [](AsyncWebServerRequest * request) {

    if (!btnPressureSwitch.isPressed()) {
      digitalWrite(pinPumpB, LOW);
      pumpBRunning = true;
      pumpBState = "ON";

      digitalWrite(pinPumpA, HIGH);
      pumpARunning = false;
      pumpAState = "OFF";

      manualON = true;
      manualControl = true;
      request->send(SPIFFS, "/index.html", String(), false, processor);
      Serial.println("Pump B On");
    }
    //showPumpScreen();
  });

  // Route to set GPIO
  server.on("/offPumpB", HTTP_GET, [](AsyncWebServerRequest * request) {
    digitalWrite(pinPumpB, HIGH);
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
    if (pumpAmps > 0.05 ) {
      pumpAThreshold = pumpAAmps / 1.125; // V139 // 1.35; // 1.7; V137 // was 1.8 & originally = 2
      if (pumpAThreshold < 0.2) pumpAThreshold = defaultThreshold; // APH was 0.2 & 2.95
      saveEEData();
      // APH157 Added Changed ZZZZZZ

      // turn it off
      digitalWrite(pinPumpA, HIGH);
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
      digitalWrite(pinPumpB, HIGH);
      pumpBRunning = false;
      pumpBState = "OFF";
      manualON = false;
      manualControl = false;
      sprintf(strMsg, "Pump B threshold set to %3.2f amps", pumpBThreshold); Serial.println(strMsg);
    }

    // APH157 Changed
    //request->send(SPIFFS, "/index.html", String(), false, processor);
    request->redirect("/");
    Serial.println("Set B");
  });
  // APH157 Added Changed
  //  server.on("/setA", HTTP_GET, [](AsyncWebServerRequest * request) {
  //
  //    sprintf(strMsg, "Pump A pumpAAmps to %3.2f amps", pumpAAmps); Serial.println(strMsg); // ??
  //
  //    // APH157 Added Changed
  //    // if (pumpAThreshold < 0.2) pumpBThreshold = 2.25; // APH was 0.2 & 2.95
  //    float foo = 12.34567;
  //    int bar = (int) foo * 1000.0; // bar now = 12345
  //    foo = (float) bar / 1000.0;   // foo now = 12.345 or something very close but that's how FP is and always has been.
  //
  //   int temp = (int) pumpAThreshold * 100.0; // bar now = 355
  //    tempThreshold = (float) temp / 100.0;   // foo now = 3.555 or something very close but that's how FP is and always has been.
  //   sprintf(strMsg, "Pump A %3.2f amps", tempThreshold); Serial.println(strMsg); // ??
  //
  //    if (pumpAAmps != 0 || (int)pumpAThreshold != (4 / 1.125)) { // Only change threshold if page refreeshed and pump IS running
  //      pumpAThreshold = pumpAAmps / 1.125; // V139 // 1.35; // 1.7; // was 2 V137
  //      if (pumpAThreshold < 0.2) pumpAThreshold = 2.25; // APH was 0.2 & 2.95
  //    }
  //    // APH157 Added Changed
  //    saveEEData();
  //
  //    // turn it off
  //    digitalWrite(pinPumpA, HIGH);
  //    pumpARunning = false;
  //    pumpAState = "OFF";
  //    manualON = false;
  //    manualControl = false;
  //    sprintf(strMsg, "Pump A threshold set to %3.2f amps", pumpAThreshold); Serial.println(strMsg);
  //
  //    request->send(SPIFFS, "/index.html", String(), false, processor);
  //
  //    Serial.println("Set A");
  //  });
  // APH157 Added Changed

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
    digitalWrite(pinPumpB, HIGH);
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
      digitalWrite(pinPumpA, HIGH);
      pumpARunning = false;
      pumpAState = "OFF";
      Serial.println("Pump A Stopped");
      digitalWrite(pinPumpB, LOW);
      pumpBRunning = true;
      pumpBState = "ON";
      Serial.println("Started Pump B");

    }
    else if (pumpBRunning) {
      digitalWrite(pinPumpB, HIGH);
      pumpBRunning = false;
      pumpBState = "OFF";
      Serial.println("Pump B Stopped");
      digitalWrite(pinPumpA, LOW);
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

#ifdef useRTC
      rtc.adjust(DateTime(2020, 4, 29, hh, mm, 0));
#endif
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
#endif
  WiFi.setSleep(false);
  server.begin();

  ledcSetup(0, 1000, 10);

// APH 157 added as only used on dev board
#ifdef DEV_BOARD
  ledcAttachPin(pinBuzzer, 0);
#endif
// APH 157 added as only used on dev board

  if (btnPressureSwitch.isPressed()) {
    // APH 157  digitalWrite(pinPSLED, HIGH);
    tapOpen = true;
    Serial.println("Tap is OPEN");
    // APH 155 changed

    //    digitalWrite(pinPumpA, LOW); // start A
    //    pumpARunning = true;
    //    pumpAState = "ON";
    //    Serial.println("Started pump A");
    // APH 155 added to check which pump last used and start with that
    if (activePump == 'A') { // insert whatever rev this is and add a comment !!!!!!!!
      digitalWrite(pinPumpA, LOW); // start A
      pumpARunning = true;
      pumpAState = "ON";
      Serial.println("Started pump A");
    }
    else {
      digitalWrite(pinPumpB, LOW); // start B
      pumpBRunning = true;
      pumpBState = "ON";
      Serial.println("Started pump B");
    }
    // APH 155 added to check which pump last used and start with that

    pumpStartTick.reset();
    pumpIsStarting = true;

    showPumpScreen();

  }
  else {
    Serial.println("Tap is CLOSED");
  }

  Serial.print("started v"); Serial.println(version);

#ifdef testing // APH added 157
  beep(1, 1, 9, 1000);
#else
  playTune(); // David likes this and used normally
#endif

  showHome();

#ifndef useRTC
  hh = 16; mm = 30; ss = 00;
#endif

  sprintf(strTime, "%02d:%02d:%02d", hh, mm, ss);
  sprintf(strMsg, "Time now : %s", strTime); Serial.println(strMsg);
  sprintf(strMsg, "Night inhibit time : %d mins", NIGHT_INHIBIT_MINS); Serial.println(strMsg);

}

void loop() {

  buttonProcessor();

  btnPressureSwitch.read();

  if (btnPressureSwitch.wasPressed()) {
    Serial.println("Tap open");
    // APH 157 digitalWrite(pinPSLED, HIGH);

    if (!pumpARunning && !pumpBRunning) {

      if (!night) {
        if (!manualControl) {

          allOff.reset(); // 138

          if (activePump == 'A') {
            digitalWrite(pinPumpA, LOW); //
            pumpARunning = true;
            pumpAState = "ON";
            Serial.println("Started pump A");
          }
          else if (activePump == 'B') {
            digitalWrite(pinPumpB, LOW); //
            pumpBRunning = true;
            pumpBState = "ON";
            Serial.println("Started pump B");
          }
          //beep(1, 1, 9, 1000);
          pumpStartTick.reset();
          pumpIsStarting = true;
          tapOpen = true;
          showPumpScreen();
        }
      }
      else {
        Serial.println("night time - no operation");
      }
    }
  }

  if (btnPressureSwitch.wasReleased() && !pumpIsStarting) {
    Serial.println("Tap close detected");
    // APH 157   digitalWrite(pinPSLED, LOW);

    if (!manualControl && !night) {
      pressurising = true;
      pressuriseTick.reset();
    }
  }

  if (pressuriseTick.check() && pressurising) {

    if (btnPressureSwitch.isReleased()) {
      pressurising = false;
      tapOpen = false;

      digitalWrite(pinPumpB, HIGH); //
      digitalWrite(pinPumpA, HIGH); //

      pumpARunning = pumpBRunning = false;
      pumpAState = "OFF"; pumpBState = "OFF";

      Serial.println("Pump off afer pressurising delay");

      showHome();
    }

  }

  if (pumpStartTick.check() && pumpIsStarting) {
    pumpIsStarting = false;

    if (pumpARunning) {
      sprintf(strMsg, "End of start phase, active pump = A, amps = %3.2f", pumpAmps);
    }
    else if (pumpBRunning) {
      sprintf(strMsg, "End of start phase, active pump = B, amps = %3.2f", pumpAmps);
    }
    Serial.println(strMsg);

    if (btnPressureSwitch.isReleased()) { // tur off at end of start if ps
      Serial.println("Tap closed at end of start, re-pressuring");
      // APH 157  digitalWrite(pinPSLED, LOW);

      if (!manualControl) {
        pressurising = true;
        pressuriseTick.reset();
      }
    }
  }

  if (secTick.check()) {
    // non RTC time stuff
    ss += 1;
    if (ss >= 60) {
      ss = 0;
      minsSinceStart += 1;
#ifndef useRTC
      mm += 1;
#endif
    }

    if (mm >= 60) {
      hh += 1;
      mm = 0;
    }
    if (hh > 23) hh = 0;

    if (minsSinceStart >= NIGHT_INHIBIT_MINS) {
      if (nightStartInhibit) {
        nightStartInhibit = false;
        Serial.println("Night inhibit off");
      }
    }

    if (sysMode == modeNormal) {

#ifdef useRTC
      DateTime now = rtc.now();
      hh = now.hour(); mm = now.minute(); ss = now.second();
#endif
      sprintf(strTime, "%02d:%02d:%02d", hh, mm, ss);
      showTime();
    }

    //sprintf(strMsg, "%s => volts now = %3.2f, amps = %3.2f, A %s, B %s", strTime, pumpVolts, pumpAmps, pumpAState.c_str(), pumpBState.c_str());
    //sprintf(strMsg, "%s => no pump volts = %3.2f, volts now = %3.2f, amps = %3.2f, A %s, B %s", strTime, noPumpVolts, pumpVolts, pumpAmps, pumpAState.c_str(), pumpBState.c_str());
    sprintf(strMsg, "Time:%02d:%02d:%02d(%d), amps: %3.2f, A:%s, B:%s, tap:%d", hh, mm, ss, night, pumpAmps, pumpAState.c_str(), pumpBState.c_str(), tapOpen);
    //Serial.println(strMsg); // *********************************

    if (pumpARunning || pumpBRunning) {
      if (display != dispPumpRunning) showPumpScreen();
      else showAmps();
    }
    else {
      if (sysMode == modeNormal) {
        if (display != dispHome) showHome();
      }
    }
    //nightStartInhibit = false;
    // check for night
    if (!nightStartInhibit) { // do it only if start and end are different

      int startMins = (nightStartHH * 60) + nightStartMM;
      int endMins = ((nightEndHH + 24) * 60) + nightEndMM;
      int minsNow = (hh * 60) + mm;

      if ((hh >= 0) && (hh <= nightEndHH)) minsNow += (24 * 60);
      //sprintf(strMsg, "start = %d, end = %d, now = %d", startMins, endMins, minsNow); Serial.println(strMsg);
      if ((minsNow >= startMins) && (minsNow < endMins)) {
        if (!night) {
          night = true;
          gDisp.screenOnOff(0); // APH Version 140 added Screen OFF at Night
          Serial.println("Night mode ON screen OFF");
          // beep(1, 1, 9, 2000); // APH added
        }
      }
      else {
        if (night) {
          night = false;
          gDisp.screenOnOff(1); // APH Version 140 added Screen ON during day
          if (btnPressureSwitch.isPressed()) {
            Serial.println("Night end - pressurising");

            if (activePump == 'A') {
              digitalWrite(pinPumpA, LOW); //
              pumpARunning = true;
              pumpAState = "ON";
              Serial.println("Started pump A");
            }
            else if (activePump == 'B') {
              digitalWrite(pinPumpB, LOW); //
              pumpBRunning = true;
              pumpBState = "ON";
              Serial.println("Started pump B");
            }

            pressuriseTick.reset();
            pressurising = true;
          }
          else {
            Serial.println("Night end - already pressurised");
          }
        }
      }

    }

    // diagnostic stuff
    if (tapOpen) {
      if (pumpARunning) {
        sprintf(strMsg, "TapOpen Pump A %3.2f amps", pumpAAmps);
        Serial.println(strMsg);
      }
      else if (pumpBRunning) {
        sprintf(strMsg, "TapOpen Pump B %3.2f amps", pumpBAmps);
        Serial.println(strMsg);
      }
    }
  }

  if (sampleTick.check()) {

    pumpSamples[sampleIndex] = analogRead(pinPumpCurrent);
    sampleIndex += 1;
    if (sampleIndex >= MAX_SAMPLES) sampleIndex = 0;

    int aggregate = 0;

    for (int i = 0; i < MAX_SAMPLES; i++) aggregate += pumpSamples[i];

    pumpVolts = (double)aggregate * 4.74 / 701 / MAX_SAMPLES;
    //pumpVolts = (double)aggregate * 2.35 / 697 / MAX_SAMPLES; // ***************************

    if (!pumpARunning && !pumpBRunning && pumpVolts > 0.1) noPumpVolts = pumpVolts;
    else noPumpVolts = 2.35;

    pumpAmps = (noPumpVolts - pumpVolts) * 4.5;

    if (pumpARunning) {

#ifdef USE_BUTTONS_FOR_PS_AND_PUMP
      if (outOfWater) pumpAAmps = 0;
      else pumpAAmps = 5;
      pumpAmps = pumpAAmps;
#else
      pumpAAmps = (noPumpVolts - pumpVolts) * 4.5;
#endif

      if (!inTransition && (pumpAAmps < pumpAThreshold) && !manualControl && !pumpIsStarting && !pressurising) {
        digitalWrite(pinPumpA, HIGH); // turn A off //
        pumpARunning = false;
        pumpAState = "OFF";

        inTransition = true;
        transitionTick.reset();
        Serial.println("Changing A to B");

        digitalWrite(pinPumpB, LOW); // and B on //
        pumpBRunning = true;
        pumpBState = "ON";
        activePump = 'B';

        EEPROM.write(EE_ACTIVE_PUMP, activePump);
        EEPROM.commit();

        beep(2, 1, 5, 1500); // APH beep was beep(2, 1, 9, 1000)

        refillPending = true;

        showPumpScreen();
      }
    }
    else pumpAAmps = 0;

    if (pumpBRunning) {

#ifdef USE_BUTTONS_FOR_PS_AND_PUMP
      if (outOfWater) pumpBAmps = 0;
      else pumpBAmps = 5;
      pumpAmps = pumpBAmps;
#else
      pumpBAmps = (noPumpVolts - pumpVolts) * 4.5;
#endif

      if (!inTransition && (pumpBAmps < pumpBThreshold) && !manualControl && !pumpIsStarting && !pressurising) {
        digitalWrite(pinPumpB, HIGH); // turn B off //
        pumpBRunning = false;
        pumpBState = "OFF";

        inTransition = true;
        transitionTick.reset();
        Serial.println("Changing B to A");

        digitalWrite(pinPumpA, LOW); // and A on //
        pumpARunning = true;
        activePump = 'A';
        pumpAState = "ON";

        EEPROM.write(EE_ACTIVE_PUMP, activePump);
        EEPROM.commit();

        beep(2, 1, 5, 1500); // APH beep was beep(2, 1, 9, 1000)

        refillPending = true;

        showPumpScreen();
      }
    }
    else pumpBAmps = 0;
  }

  if (tenthTick.check()) {
    if (onCount) {
      onCount -= 1;

      if (onCount == 0) {
        ledcWriteTone(0, 0);
        digitalWrite(pinNewBuzzer, 0);
        offCount = offTime + 1;
      }
    }

    if (offCount) {
      offCount -= 1;

      if (offCount == 0) {
        beepCount -= 1;

        if (beepCount) {
          ledcWriteTone(0, beepFreq);
          digitalWrite(pinNewBuzzer, 1);
          onCount = onTime;
        }
        else {
          beeping = false;
        }
      }
    }
  }

  if (clickTick.check() && !beeping) {
    ledcWriteTone(0, 0);
    // 142
    digitalWrite(pinNewBuzzer, 0);
  }

  if (transitionTick.check() && !manualControl) {
    if (inTransition) {
      inTransition = false;
      Serial.println("Transition time over");

      // check state after 5 secs to see if new barrel has water or not
      if (pumpARunning) {
        if (pumpAAmps < pumpAThreshold) {
          digitalWrite(pinPumpA, HIGH); // turn A off //
          pumpARunning = false;
          pumpAState = "OFF";
          Serial.println("All off - no water (A was running)");
          //??    beep(4, 2, 8, 500); // APH beep was beep(3, 1, 9, 1000)
          showHome();
        }
      }
      else if (pumpBRunning) {
        if (pumpBAmps < pumpBThreshold) {
          digitalWrite(pinPumpB, HIGH); // turn B off //
          pumpBRunning = false;
          pumpBState = "OFF";
          Serial.println("All off - no water (B was running)");
          //??    beep(4, 2, 8, 500); // APH beep was beep(3, 1, 9, 1000)
          showHome();
        }
      }
    }
  }

  if (allOff.check()) { // 138
    if (pumpARunning || pumpBRunning) {
      Serial.println("All off - 3 minute timeout");
      // pump A
      digitalWrite(pinPumpA, HIGH); // turn A off //
      pumpARunning = false;
      pumpAState = "OFF";

      //pump B
      digitalWrite(pinPumpB, HIGH); // turn B off //
      pumpBRunning = false;
      pumpBState = "OFF";

      showHome();
    }
  }

  if (changeDisplayTick.check() && changeDisplay) {
    if (pumpARunning || pumpBRunning) showPumpScreen();
    else showHome();

    changeDisplay = false;
  }

  if (longPress.check() && btnScreen.isPressed()) { // 152
    longPressCount += 1;
    if (longPressCount == 3) beep(2, 1, 1, 0); // 153 was 1 beep
    if (longPressCount == 5) beep(3, 1, 1, 0);
  }
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
// APH157 Added Changed
//String processor(const String& var) {
//  //Serial.println(var);
//
//  if (var == "STATEA") {
//    if (!digitalRead(pinPumpA)) ledState = "ON"; //
//    else ledState = "OFF";
//    return ledState;
//  }
//
//  else if (var == "STATEB") {
//    if (!digitalRead(pinPumpB)) ledState = "ON"; //
//    else ledState = "OFF";
//    return ledState;
//  }
//
//  else if (var == "ampsA") {
//    return String(pumpAAmps).c_str();
//  }
//
//  else if (var == "ampsB") {
//    return String(pumpBAmps).c_str();
//  }
//
//  else if (var == "time") {
//    return getTime();
//  }
//
//  else if (var == "systemOnTime") { // I have changed from currentontime
//    char str[5];
//    sprintf(str, "%02d:%02d", nightEndHH, nightEndMM); // APH added ':' was sprintf(str, "%02d%02d", nightEndHH, nightEndMM);
//    return String(str);
//  }
//
//  else if (var == "systemOffTime") { // I have changed from currentofftime
//    char str[5];
//    sprintf(str, "%02d:%02d", nightStartHH, nightStartMM); // APH added ':'
//    return String(str);
//  }
//
//  // APH 156 added version
//  //   else if (var == "version") {
//  //      return String(version).c_str();
//  //  }
//  // APH 156
//
//  return String();
//}
// APH157 Added Changed

String getTime() {
  char strTime[10];
  sprintf(strTime, "%02d:%02d", hh, mm);
  String str(strTime);
  //Serial.println(str);
  return str;
}

void saveEEData() {
  union {
    byte b[2];
    uint16_t d;
  } data;

  // add 10%
  data.d = (uint16_t)(100 * pumpAThreshold);
  EEPROM.write(EE_A_THRESHOLD, data.b[0]);
  EEPROM.write(EE_A_THRESHOLD + 1, data.b[1]);
  //sprintf(strMsg, "Write : b[0] = %d, b[1] = %d", data.b[0], data.b[1]); Serial.println(strMsg);

  data.d = (uint16_t)(100 * pumpBThreshold);
  EEPROM.write(EE_B_THRESHOLD, data.b[0]);
  EEPROM.write(EE_B_THRESHOLD + 1, data.b[1]);
  //sprintf(strMsg, "Write : b[0] = %d, b[1] = %d", data.b[0], data.b[1]); Serial.println(strMsg);

  // times
  EEPROM.write(EE_NIGHT_START_HH, (byte)nightStartHH);
  EEPROM.write(EE_NIGHT_START_MM, (byte)nightStartMM);

  EEPROM.write(EE_NIGHT_END_HH, (byte)nightEndHH);
  EEPROM.write(EE_NIGHT_END_MM, (byte)nightEndMM);

  EEPROM.commit();

  getEEData();
}

void getEEData() {
  union {
    byte b[2];
    uint16_t d;
  } data;

  data.b[0] = EEPROM.read(EE_A_THRESHOLD);
  data.b[1] = EEPROM.read(EE_A_THRESHOLD + 1);
  pumpAThreshold = (double)data.d / 100;
  // APH157 Added ZZZZZZ
  // sprintf(strMsg, "Pump A threshold retrieved %3.2f", pumpAThreshold);
  if (pumpAThreshold < defaultThreshold * 0.5 || pumpAThreshold >= defaultThreshold * 2.5) pumpAThreshold = defaultThreshold;
  // if (pumpAThreshold < 0.2 || pumpAThreshold >= 5) pumpAThreshold = defaultThreshold; // APH157 Added Changed
  // APH157 Added Changed

  data.b[0] = EEPROM.read(EE_B_THRESHOLD);
  data.b[1] = EEPROM.read(EE_B_THRESHOLD + 1);
  pumpBThreshold = (double)data.d / 100;
  //sprintf(strMsg, "Read : b[0] = %d, b[1] = %d", data.b[0], data.b[1]); Serial.println(strMsg);

  // APH157 Added
  if (pumpBThreshold < defaultThreshold * 0.5 || pumpBThreshold >= defaultThreshold * 2.5) pumpBThreshold = defaultThreshold;
  // if (pumpBThreshold < 0.2 || pumpBThreshold >= 5) pumpBThreshold = defaultThreshold; // APH157 Added Changed
  // APH157 Added Changed

  sprintf(strMsg, "Thresholds: Pump A = %3.2fA, Pump B = %3.2fA", pumpAThreshold, pumpBThreshold); Serial.println(strMsg);

  nightStartHH = (int)EEPROM.read(EE_NIGHT_START_HH);
  if (nightStartHH > 23) nightStartHH = 22;

  nightStartMM = (int)EEPROM.read(EE_NIGHT_START_MM);
  if (nightStartMM > 59) nightStartMM = 30;

  nightEndHH = (int)EEPROM.read(EE_NIGHT_END_HH);
  //  if ((nightEndHH > 10) || (nightEndHH == 0)) nightEndHH = 6;
  //  if (nightEndHH > 10) nightEndHH = 6; // APH

  nightEndMM = (int)EEPROM.read(EE_NIGHT_END_MM);
  if (nightEndMM > 59) nightStartMM = 30;
  //nightEndHH = 0; nightEndMM = 2;

  // APH Added to retrieve last pump
  // APH157 Added
  activePump = (char)EEPROM.read(EE_ACTIVE_PUMP);
  sprintf(strMsg, "Active PUMP %c", activePump); Serial.println(strMsg);
  // APH157 Added Changed
  if ((activePump != 'A') && (activePump != 'B')) {
    activePump = 'A';
    EEPROM.write(EE_ACTIVE_PUMP, 'A');
    EEPROM.commit();
  }
  // APH Added

  if (activePump == 'A') Serial.println("Pump A active");
  else Serial.println("Pump B active");

  sprintf(strMsg, "Night start = %02d:%02d, night end  = %02d:%02d", nightStartHH, nightStartMM, nightEndHH, nightEndMM); Serial.println(strMsg);
}

// sound *****************

void playTune() {

  for (int thisNote = 0; thisNote < 8; thisNote++) {

    int noteDuration = 1000 / noteDurations[thisNote];
    ledcWriteTone(0, melody[thisNote]);

    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);

    ledcWriteTone(0, 0);
  }
}

void beep(int count, int duration, int spacing, int freq) {
  onTime = duration;
  onCount = onTime;
  offTime = spacing;
  beepCount = count;
  tenthTick.reset();
  beepFreq = freq;
  ledcWriteTone(0, beepFreq);
  digitalWrite(pinNewBuzzer, 1);
  beeping = true;
}

void click() {
  clickTick.reset();
  ledcWriteTone(0, clickFreq);

  // 142
  digitalWrite(pinNewBuzzer, 1);
}

// end of sound

// display **************************
void showHome() { // ZZZ
  manualON = false;
  manualControl = false;
  display = dispHome;

  gDisp.clearScreen();
  // APH gDisp.setColor(RED); gDisp.drawFrame(0, 0, 127, 127);

  if (!refillPending) {
    gDisp.setColor(WHITE);

    if (activePump == 'A') gDisp.drawBitmap(0, 0, 125, 64, FRONT); // 32
    else gDisp.drawBitmap(0, 0, 125, 64, REAR); // APH move position slightly 151

    gDisp.drawBitmap(0, 64, 125, 64, PUMP); // APH 151
  }
  else {
    showCross(RED);
  }
  // APH 153 Added
  gDisp.setFont(202);
  // APH    gDisp.setFont(fonts[3]); // 0, 1, 2, 3,
  gDisp.setColor(TURQUOISE);
  if (APActive) {
    gDisp.setTextPosAbs(46, 78);
    gDisp.print("WIFI");
  } else {
    gDisp.setTextPosAbs(46, 78);
    gDisp.print("");
  }
  // APH 153

  showTime();
}

void showTime() {

  if (extensionMode) {
    return;  // APH 145 ZZZ
  }

  int ypos = 118, xpos = 13;

  gDisp.setColor(BLACK);
  gDisp.setFont(202);

  if (sysMode == modeNormal) {

    if ((mm == 0) && (ss == 0)) gDisp.drawBox(xpos, ypos - 30, 120 - xpos, 30);
    else if (ss == 0) gDisp.drawBox(xpos - 2, ypos - 30, 12, 36);
    else gDisp.drawBox(xpos + 26, ypos - 30, 80, 36);

    gDisp.setColor(YELLOW);
    gDisp.setTextPosAbs(xpos, ypos);
    gDisp.print(strTime);

  }
  else {
    xpos += 5;
    gDisp.drawBox(xpos - 15, ypos - 30, 125 - xpos, 32);
    gDisp.setColor(YELLOW);

    char strHH[4], strMM[4];

    if (sysMode == modeSetTime) {
      sprintf(strHH, "%02d", hh);
      sprintf(strMM, "%02d", mm);
    }
    else if (sysMode == modeSetNightStart) {
      sprintf(strHH, "%02d", nightStartHH);
      sprintf(strMM, "%02d", nightStartMM);
    }
    else if (sysMode == modeSetNightEnd) {
      sprintf(strHH, "%02d", nightEndHH);
      sprintf(strMM, "%02d", nightEndMM);
    }

    gDisp.setTextPosAbs(xpos, ypos);
    gDisp.print(strHH);

    gDisp.setTextPosAbs(xpos + 30, ypos);
    gDisp.print(":");

    gDisp.setTextPosAbs(xpos + 35, ypos);
    gDisp.print(strMM);

    if (setHours) gDisp.drawFrame(xpos - 2, ypos - 30, 28, 32);
    if (setMins) gDisp.drawFrame(xpos + 33, ypos - 30, 28, 32);
  }

}
void showPumpScreen() { // This is when a Pump is running
  display = dispPumpRunning;

  gDisp.clearScreen();
  // APH  gDisp.setColor(RED); gDisp.drawFrame(0, 0, 127, 127);

  //?? gDisp.setColor(GREEN);
  gDisp.setColor(TURQUOISE);
  if (pumpARunning) gDisp.drawBitmap(0, 0, 125, 64, FRONT); // 147
  else if (pumpBRunning) gDisp.drawBitmap(0, 0, 125, 64, REAR);

  //  gDisp.drawBitmap(2, 32, 125, 64, RUNNING);
  gDisp.drawBitmap(0, 64, 125, 64, RUNNING);

  gDisp.setFont(202);

  int xpos = 10, ypos = 110;

  if (!extensionMode) {
    if (!manualControl) {
      xpos = 40;
      gDisp.setTextPosAbs(xpos, ypos);
      gDisp.print("TAP");
    }
    else {
      gDisp.print("BUTTON");
    }
  }
}

// APH Version 140
void showCross(uint8_t col) {

  gDisp.clearScreen();
  // APH  gDisp.setColor(RED); gDisp.drawFrame(0, 0, 127, 127);

  // APH157 Added to stop word Running
  pumpARunning = false;
  pumpBRunning = false;
  // APH157 Added

  //  APH 155 move text to 2 rows by adding word empty to improve layout
  int height = 127, picPos = 0; // APH 155 was picPos = 32;

  if (!extensionMode) {
    height = 80;
    picPos = 30;
  }

  gDisp.setColor(col);
  // Top Left to Bottom Right
  for (int i = 0; i < 9; i++) gDisp.drawLine(i, 0, 127, height - i);
  for (int i = 0; i < 9; i++) gDisp.drawLine(0, i, 127 - i, height);

  // Bottom Left to Top Right
  for (int i = 0; i < 9; i++) gDisp.drawLine(0, height - i, 127 - i, 0);
  for (int i = 0; i < 9; i++) gDisp.drawLine(i, height, 127, i);

  gDisp.setColor(WHITE);

  if (activePump == 'A') {
    gDisp.drawBitmap(0, picPos, 125, 64, REAR);
  }
  else {
    gDisp.drawBitmap(0, picPos, 125, 64, FRONT);
  }
  // APH 155 added
  gDisp.drawBitmap(0, 64, 125, 64, EMPTY); // APH 157
  // gDisp.drawBitmap(0, 60, 127, 92, empty3); // APH 157
  // APH 155
}
// APH Version


void showAmps() {

  if (extensionMode) {
    return;  // APH 145 ZZZ
  }

  int xpos = 5, ypos = 90;

  gDisp.setColor(BLACK);
  gDisp.drawBox(xpos, ypos - 30, 124 - xpos, 30);

  gDisp.setTextPosAbs(xpos, ypos);
  gDisp.setFont(202);
  gDisp.setColor(TURQUOISE);

  if (pumpARunning) sprintf(strMsg, "AMPS = %3.2f", pumpAAmps);
  else sprintf(strMsg, "AMPS = %3.2f", pumpBAmps);

  gDisp.print(strMsg);
}

// end of display *************************************

// buttons *****************************
void buttonProcessor() {

  // APH Version 140 Added new Extension display Button
  // if (!pumpARunning || !pumpBRunning) {
  btnScreen.read();

  if (btnScreen.wasPressed()) {
    Serial.println("Screen button pressed");

    startMillis = millis();
    beep(1, 1, 1, 0);

    longPress.reset();
    longPressCount = 0;
  }

  if (btnScreen.wasReleased()) {

    if (ignoreFirstRelease) {
      ignoreFirstRelease = false;
      return;
    }

    Serial.println("Screen button released");

    if (millis() - startMillis < 1000) { // 152 remove cross
      if (refillPending) {
        refillPending = false;
        Serial.println("refill cleared");
      }
    }
    else if (millis() - startMillis >= 3000 && millis() - startMillis < 5000) { // wifi 152

      if (!APActive) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(APssid.c_str(), APpassword.c_str());
        //WiFi.softAPsetHostname(APssid.c_str());
        //WiFi.softAPConfig(localIP, gateway, subnet);
        delay(100);
        Serial.println(); Serial.print(APssid); Serial.println(" access point started");

        Serial.println();
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());

        APActive = true;
      }
      else {
        WiFi.softAPdisconnect(true);
        WiFi.enableAP(false);

        APActive = false;
        Serial.println("access point off");
      }
    }
    else if (millis() - startMillis > 5000) { // Swap pumps 152
      if (activePump == 'A') {
        activePump = 'B';
        Serial.println("Pump B now active");
      }
      else {
        activePump = 'A';
        Serial.println("Pump A now active");
      }
    }

    showHome();
  }

#ifdef USE_BUTTONS_FOR_PS_AND_PUMP
  btnTankOutOfWater.read();

  if (btnTankOutOfWater.wasPressed()) {
    outOfWater = true;
    Serial.println("out of water, amps = 0");
  }

  if (btnTankOutOfWater.wasReleased()) {
    outOfWater = false;
    Serial.println("amps = 5");
  }

#else

  btnSet.read();
  btnA.read();
  btnB.read();

  if (btnSet.wasPressed()) {
    Serial.println("button Set");
    click();
    if (sysMode == modeNormal) {
      if (pumpARunning || pumpBRunning) {
        if (pumpARunning) {
          pumpAThreshold = pumpAAmps / 2; // 123
          if (pumpAThreshold < 0.2) pumpAThreshold = defaultThreshold; // APH157 Changed was 0.2;
          sprintf(strMsg, "Pump A threshold set to %3.2f", pumpAThreshold);
        }
        else if (pumpBRunning) {
          pumpBThreshold = pumpBAmps / 2; // 123
          if (pumpBThreshold < 0.2) pumpBThreshold = 0.2;
          sprintf(strMsg, "Pump B threshold set to %3.2f", pumpBThreshold);
        }

        saveEEData();
      }
      else {
        sysMode = modeSetTime;

        gDisp.clearScreen();

        gDisp.setFont(202);
        gDisp.setTextPosAbs(25, 50);
        gDisp.setColor(RED);
        gDisp.print("SET TIME");

        setHours = true;
        setMins = false;

        showTime();
      }
    }
    else {
      if (sysMode == modeSetTime) {
        if (setHours) {
          setHours = false;
          setMins = true;
          showTime();
        }
        else if (setMins) {
          setMins = false;
          setHours = true;
          sysMode = modeSetNightStart;

          gDisp.clearScreen();
          gDisp.setFont(202);
          gDisp.setTextPosAbs(2, 50);
          gDisp.setColor(RED);
          gDisp.print("NIGHT START");

          showTime();
        }
      }
      else if (sysMode == modeSetNightStart) {
        if (setHours) {
          setHours = false;
          setMins = true;
          showTime();
        }
        else if (setMins) {
          setMins = false;
          setHours = true;
          sysMode = modeSetNightEnd;

          gDisp.clearScreen();
          gDisp.setFont(202);
          gDisp.setTextPosAbs(20, 50);
          gDisp.setColor(RED);
          gDisp.print("NIGHT END");

          showTime();
        }
      }
      else if (sysMode == modeSetNightEnd) {
        if (setHours) {
          setHours = false;
          setMins = true;
          showTime();
        }
        else if (setMins) {
          setMins = false;
          setHours = true;
          sysMode = modeNormal;

          saveEEData();

          sprintf(strMsg, "Night is now from %02d:%02d to %02d:%02d", nightStartHH, nightStartMM, nightEndHH, nightEndMM);
          Serial.println(strMsg);
          showHome();
        }
      }
    }
  }
  if (btnA.wasPressed()) {
    Serial.println("button A");
    click();

    if (btnPressureSwitch.isPressed()) return;

    if (sysMode == modeNormal) {
      if (!pumpARunning) {
        pumpBRunning = false;
        digitalWrite(pinPumpB, HIGH); //
        Serial.println("Started A");

        pumpARunning = true;
        pumpAState = "on";
        digitalWrite(pinPumpA, LOW); //
        manualON = true;
        manualControl = true;
        showPumpScreen();
      }
      else {
        pumpARunning = false;
        pumpAState = "off";
        digitalWrite(pinPumpA, HIGH); //
        manualControl = false;
        showHome();
      }
    }
    else {
      if (sysMode == modeSetTime) {
        if (setHours) {
          hh += 1;
          if (hh == 24) hh = 0;
        }
        else if (setMins) {
          mm += 1;
          if (mm == 60) mm = 0;
        }

#ifdef useRTC
        rtc.adjust(DateTime(2020, 4, 29, hh, mm, 0));
#endif
        sprintf(strMsg, "Time set to %02d:%02d", hh, mm); Serial.println(strMsg);
      }
      else if (sysMode == modeSetNightStart) {
        if (setHours) {
          nightStartHH += 1;
          if (nightStartHH == 24) nightStartHH = 0;
        }
        else if (setMins) {
          nightStartMM += 1;
          if (nightStartMM == 60) nightStartMM = 0;
        }
      }
      else if (sysMode == modeSetNightEnd) {
        if (setHours) {
          nightEndHH += 1;
          if (nightEndHH == 24) nightEndHH = 0;
        }
        else if (setMins) {
          nightEndMM += 1;
          if (nightEndMM == 60) nightEndMM = 0;
        }
      }
      showTime();
    }
  }

  if (btnB.wasPressed()) {
    Serial.println("button B");
    click();

    if (btnPressureSwitch.isPressed()) return;

    if (sysMode == modeNormal) {
      if (!pumpBRunning) {
        pumpARunning = false;
        digitalWrite(pinPumpA, HIGH); //

        pumpBRunning = true;
        pumpBState = "on";
        digitalWrite(pinPumpB, LOW); //
        manualON = true;
        manualControl = true;
        Serial.println("Started B");

        showPumpScreen();
      }
      else {
        pumpBRunning = false;
        pumpBState = "off";
        manualControl = false;
        digitalWrite(pinPumpB, HIGH); //

        showHome();
      }
    }
    else {
      if (sysMode == modeSetTime) {
        if (setHours) {
          hh -= 1;
          if (hh < 0) hh = 23;
        }
        else if (setMins) {
          mm -= 1;
          if (mm < 0) mm = 59;
        }
#ifdef useRTC
        rtc.adjust(DateTime(2020, 4, 29, hh, mm, 0));
#endif
      }
      else if (sysMode == modeSetNightStart) {
        if (setHours) {
          nightStartHH -= 1;
          if (nightStartHH < 0) nightStartHH = 23;
        }
        else if (setMins) {
          nightStartMM -= 1;
          if (nightStartMM < 0) nightStartMM = 59;
        }
      }
      else if (sysMode == modeSetNightEnd) {
        if (setHours) {
          nightEndHH -= 1;
          if (nightEndHH < 0) nightEndHH = 23;
        }
        else if (setMins) {
          nightEndMM -= 1;
          if (nightEndMM < 0) nightEndMM = 59;
        }
      }
      showTime();
    }
  }
#endif
}