/*******************************************************************
    THis is a fork of:
    A touch screen test for the ESP32 Cheap Yellow Display.

    https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display

    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/

    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Twitter: https://twitter.com/witnessmenow

    Modified by Simon Walters (@cymplecy@fosstodon.org)
    Adds ability to publish and subscribe to an MQTT broker 
    using MQTT and JSON
 *******************************************************************/

// Make sure to copy the UserSetup.h file into the library as
// per the Github Instructions. The pins are defined in there.

// ----------------------------
// Standard Libraries
// ----------------------------

#include <SPI.h>


// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <XPT2046_Touchscreen.h>
// A library for interfacing with the touch screen
//
// Can be installed from the library manager (Search for "XPT2046")
//https://github.com/PaulStoffregen/XPT2046_Touchscreen

#include <TFT_eSPI.h>
// A library for interfacing with LCD displays
//
// Can be installed from the library manager (Search for "TFT_eSPI")
//https://github.com/Bodmer/TFT_eSPI


// ----------------------------
// Touch Screen pins
// ----------------------------

// The CYD touch uses some non default
// SPI pins

#include <WiFi.h>
#include <esp_task_wdt.h>

#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#include <ArduinoJson.h>

// create a file secret.h in the same folder as this sketch to hold your own values
#include "secret.h"
const char ssid[] = SSID;
const char password[] = PASSWORD;
byte mqtt_server[] = MQTT_SERVER;
#define mqtt_port 1883
// ----------------------------

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

//ESP32 Only but safe to leave in for all devices
#define WDT_TIMEOUT    10

SPIClass mySpi = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();
WiFiClient wifiClient;
PubSubClient client(wifiClient);
String cheerlights = "Not Set yet";
word cheer16bit = 65535;
DynamicJsonDocument  doc(200);
DynamicJsonDocument  rcvdDoc(256);

void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.print("WiFi connected.  ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "CYDClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      //Once connected,...
      // ... and resubscribe
      client.subscribe("CYD1/fromNR",0);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte *payload, unsigned int length) {
  Serial.println("sub rcvd:");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  rcvdDoc.clear();
  deserializeJson(rcvdDoc, (const byte*)payload, length);
  
  if (rcvdDoc.containsKey("rgb565Decimal")) {
    cheer16bit = rcvdDoc["rgb565Decimal"];  
    Serial.println(cheer16bit);
  }
  
  if (rcvdDoc.containsKey("drawLine")) {
    int xStart = rcvdDoc["xStart"];
    int yStart = rcvdDoc["yStart"];
    int xEnd = rcvdDoc["xEnd"];
    int yEnd = rcvdDoc["yEnd"];
    word lineColour = rcvdDoc["lineColour"];
    tft.drawLine(xStart, yStart, xEnd, yEnd, lineColour);
    Serial.println("drawLine");
  }
  
  if (rcvdDoc.containsKey("elipse")) {
    int x = rcvdDoc["x"];
    int y = rcvdDoc["y"];
    int rx = 0;
    int ry = 0;
    if (rcvdDoc.containsKey("r")) {
      rx = rcvdDoc["r"];
      ry = rcvdDoc["r"];
    } else {
      rx = rcvdDoc["rx"];
      ry = rcvdDoc["ry"];
    }
    word fillColour = rcvdDoc["fillColour"];
    tft.fillEllipse(x, y, rx, ry, fillColour);
  }
  
  if (rcvdDoc.containsKey("writeText")) {
    int x = rcvdDoc["x"];
    int y = rcvdDoc["y"];
    String temp = rcvdDoc["writeText"];
    word backgroundColour = TFT_BLACK;
    if (rcvdDoc.containsKey("backgroundColour")) {
      backgroundColour = rcvdDoc["backgroundColour"];
    }
    word textColour = TFT_WHITE;
    if (rcvdDoc.containsKey("textColour")) {
      textColour = rcvdDoc["textColour"];
    }
    tft.setTextColor(textColour,backgroundColour);
    int fontSize = 4;
    if (rcvdDoc.containsKey("fontSize")) {
      fontSize = rcvdDoc["fontSize"];
    }
    tft.drawString(temp, x, y, fontSize);
  }
  
  if (rcvdDoc.containsKey("clear")) {
    tft.fillScreen(TFT_BLACK);
  }

}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
  // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(1);

  // Start the tft display and set it to black
  tft.init();
  tft.setRotation(1); //This is the display in landscape

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  int x = 320 / 2; // center of display
  int y = 200;
  int fontSize = 2;
  tft.drawCentreString("Touch Screen to Start", x, y, fontSize);
}

void printTouchToSerial(TS_Point p) {
  Serial.print("Pressure = ");
  Serial.println(p.z);
//  Serial.print(", x = ");
//  Serial.print(p.x);
//  Serial.print(", y = ");
//  Serial.print(p.y);
//  Serial.println();
  // doc previously declard as DynamicJsonDocument  doc(200);
  doc.clear();
  doc["pressure"] = p.z;
  doc["x"] = p.x;
  doc["y"] = p.y;
  char buffer[256];
  serializeJson(doc, buffer);
  client.publish("CYD1", buffer);

}

void printTouchToMQTT(TS_Point p) {
  Serial.print("Pressure = ");
  Serial.println(p.z);
  // doc previously declard as DynamicJsonDocument  doc(200);
  doc.clear();
  doc["pressure"] = p.z;
  doc["x"] = p.x;
  doc["y"] = p.y;
  char buffer[256];
  serializeJson(doc, buffer);
  client.publish("CYD1", buffer);
}

void printTouchToDisplay(TS_Point p) {

  // Clear screen first
  tft.fillScreen(cheer16bit);
  tft.setTextColor(TFT_BLACK,cheer16bit);

  int x = 320 / 2; // center of display
  int y = 100;
  int fontSize = 2;

  String temp = "Pressure = " + String(p.z);
  tft.drawCentreString(temp, x, y, fontSize);

  y += 16;
  temp = "X = " + String(p.x);
  tft.drawCentreString(temp, x, y, fontSize);

  y += 16;
  temp = "Y = " + String(p.y);
  tft.drawCentreString(temp, x, y, fontSize);
  y += 16;
  temp = "Colour = " + cheerlights;
  tft.drawCentreString(temp, x, y, fontSize);
}

void loop() {
  client.loop();
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    printTouchToMQTT(p);
    //printTouchToDisplay(p);
    delay(100);
  }
}
