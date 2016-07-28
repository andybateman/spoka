
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
#include <ESP8266mDNS.h>       // Also calls ESP8266WiFi.h and WiFiUdp.h
#include <Adafruit_NeoPixel.h> // Also calls Arduino.h

const char* OTAName = "esp12e-spoka-dev";    // Name of device as displayed in Arduino
const char* OTAPassword = "cf506a3aa";       // Password for Arduino to proceed with upload

#define buttonPin 13           // Define pin for mode switch (pulled up)
#define pixelPin 4             // Define pin that the data line for first NeoPixel
#define pixelCount 2           // How many NeoPixels are you using?

byte colourPos = 30;           // Default Colour (NB: 30 is Yellow, 150 is a nice Blue)

// NTP Servers:
static const char ntpServerName[] = "time.nist.gov";  // NTP server to use
const int timeZone = 12;       // New Zealand Time

int buttonState;               // the current reading from the input pin
int lastButtonState = LOW;     // the previous reading from the input pin
long lastDebounceTime = 0;     // the last time the output pin was toggled
long debounceDelay = 50;       // the debounce time; increase if the output flickers
time_t prevDisplay = 0;        // when the digital clock was displayed
unsigned int localPort = 8888;  // local port to listen for UDP packets

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(pixelCount, pixelPin, NEO_GRB + NEO_KHZ800);

WiFiUDP Udp;
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

void setup() {
  Serial.begin(9200);
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

  pinMode(buttonPin, INPUT_PULLUP);
  pixels.begin(); // This initializes the NeoPixel library.

  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, Wheel(colourPos & 255));
  }
  pixels.show();

  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

void loop() {
  ArduinoOTA.handle();

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
    if (minute() != prevDisplay) { // update the display only if time has changed
      prevDisplay = minute();
      digitalClockDisplay();
      if (hour() >= 6 && hour() <= 18) {
        if (colourPos == 150) rainbow(134);
      } else {
        if (colourPos == 30) rainbow(119);
      }
    }
  }
}

void rainbow(uint8_t colourMove) {
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

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  if (minute() < 10) Serial.print('0');
  Serial.print(minute());
  Serial.print(" - ");
  if (hour() >= 6 && hour() <= 19) {
    Serial.print("Yellow");
  } else {
    Serial.print("Blue");
  }
  Serial.println();
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
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
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
