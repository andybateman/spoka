# HackaSpoka

Hacking an Ikea Spoka for my son, adding the functionality of a GloClock

I bought an Ikea SPÖKA (http://www.ikea.com/us/en/catalog/products/30150984/) for my Son to scare the monsters away. Originally the Spoka could just glow blue/green , I wanted it to do more.

![Image of a Spoka](https://raw.githubusercontent.com/andybateman/HackaSpoka/f115561c91b7321a314a2652e75707e52f90825c/resources/spoka.jpg)

HackaSpoka v1.6 (all about the hardware):

Simply has the little guy glowing blue from 1900 to 0600 and yellow during the day.  Will also cycle through an RGB colour wheel when pressed. Can have its firmware upgraded OTA from the Arduino IDE.

## Features

* Glows blue during the night, yellow when it's time to get up
* Replaced the three blue/green LEDs with three full RGB LEDs
* Replaced the microprcessor with an ESP8266 (esp-12e package)
* Added WIFI, everything has to have WIFI
* Replaced the power jack with a micro USB connector

## Requires

* WiFiManager
* Adafruit_NeoPixel
