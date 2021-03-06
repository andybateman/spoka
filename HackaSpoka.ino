
/*
 Spoka
 Copyright (c) 2016 Andy Bateman.  All right reserved.
 Created by Andy Bateman, June 2016.
 
 Serious bit of hacking of an Ikea SPÖKA to create a Glo Clock clone for my little boy.
 (http://www.ikea.com/us/en/catalog/products/30150984/)

*/

#include <ArduinoOTA.h>        // Also calls ESP8266WiFi.h and WiFiUdp.h
#include <WiFiManager.h>       // Also calls ESP8266WiFi.h, ESP8266WebServer.h, DNSServer.h and WiFiUdp.h

#include <TimeLib.h>           // Library to hangle the clock
#include <ESP8266WebServer.h>  // Library for the Web Server
#include <ESP8266mDNS.h>       // Also calls ESP8266WiFi.h and WiFiUdp.h
#include <Adafruit_NeoPixel.h> // Also calls Arduino.h

const char* OTAName = "SpokaBoo";      // Name of device as displayed in Arduino
const char* OTAPassword = "OTAP@ssw0rd";  // Password for Arduino to proceed with upload

#define buttonPin D1           // Define pin for mode switch (pulled up)
#define pixelPin D2             // Define pin that the data line for first NeoPixel
#define pixelCount 1           // How many NeoPixels are you using?

time_t dayStarts = 630;        // Time Spoka starts his day
time_t nightStarts = 1845;     // Time Spoka goes to bed

byte colourPos = 30;           // Default Colour (NB: 30 is Yellow, 150 is a nice Blue)

// NTP Servers:
static const char ntpServerName[] = "3.nz.pool.ntp.org";  // NTP server to use
const int timeZone = 12;       // New Zealand Time

const char* lightOnOff = "true";
int buttonState;               // the current reading from the input pin
int lastButtonState = LOW;     // the previous reading from the input pin
long lastDebounceTime = 0;     // the last time the output pin was toggled
long debounceDelay = 50;       // the debounce time; increase if the output flickers
time_t prevDisplay = 0;        // when the digital clock was displayed
time_t fourDigitTime = 0;      // Four Digit 24 Hour Clock
unsigned int localPort = 8888; // local port to listen for UDP packets

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(pixelCount, pixelPin, NEO_GRB + NEO_KHZ800);

WiFiUDP Udp;
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

ESP8266WebServer server(80);

void handleRoot() {
  char temp[400];
  const char* bgColour = "FFF700";
  if (colourPos == 150) bgColour = "0062FF";
  snprintf ( temp, 400,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='60'/>\
    <title>HackASpoka</title>\
    <style>\
      body { background-color: #%s; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; text-align: center }\
    </style>\
  </head>\
  <body>\
    <h2>Spoka thinks the time is</h2>\
    <h1>%02d:%02d<\h1>\
  </body>\
</html>",
    bgColour,hour(),minute()
  );
  server.send ( 200, "text/html", temp );
}

void handleOn() {

  char temp[400];
  pixels.setBrightness(255);
  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, Wheel(colourPos & 255));
  }
  pixels.show();
  lightOnOff = "true";
  
  snprintf ( temp, 400,

"<html>\
  <body>\
    <h1>Light On<\h1>\
  </body>\
</html>"
  );
  server.send ( 200, "text/html", temp );
}

void handleOff() {

  char temp[400];
  pixels.setBrightness(0);
  pixels.show();
  lightOnOff = "false";
  
  snprintf ( temp, 400,

"<html>\
  <body>\
    <h1>Light Off<\h1>\
  </body>\
</html>"
  );
  server.send ( 200, "text/html", temp );
}

void handleStatus() {

  char temp[400];
  
  snprintf ( temp, 400, lightOnOff );
  server.send ( 200, "text/html", temp );
}

void setup() {
  Serial.begin(9600);
  delay(250);
  
  WiFiManager wifiManager;
  wifiManager.autoConnect();

  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("HTTP server started");

  pinMode(buttonPin, INPUT_PULLUP);
  pixels.begin(); // This initializes the NeoPixel library.

  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, Wheel(colourPos & 255));
  }
  pixels.show();

  Udp.begin(localPort);
  Serial.print("NTP Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(10);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      if (buttonState = reading == HIGH) {
        rainbow(254);
      }
    }
   }
  lastButtonState = reading;

  if (timeStatus() != timeNotSet) {
    fourDigitTime = (hour()*100) + minute();
    if (fourDigitTime != prevDisplay) { // update the display only if time has changed
      Serial.print(prevDisplay = fourDigitTime);
      
      if (fourDigitTime >= dayStarts && fourDigitTime < nightStarts) {
        Serial.print(" - Yellow");
        if (colourPos == 150) rainbow(134);
      } else {
        Serial.print(" - Blue");
        if (colourPos == 30) rainbow(119);
      }
      Serial.println();
    }
  }
}

void rainbow(uint8_t colourMove) {
  
  pixels.setBrightness(255);
  lightOnOff = "true";
  
  for(uint16_t j=0; j<=colourMove; j++) {

    colourPos++;
    if (colourPos>=255) { colourPos = 0; }
    
    for(uint16_t i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel(colourPos & 255));
    }
    pixels.show();
    delay(15);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.

int32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      setSyncInterval(1200);
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("NTP Server Not Responding");
  return 0;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
