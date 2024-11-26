# Laser Beacon

The Laser Beacon is a Python project of location sensor implemented on 2 laser sensor range finders. The lasers are connected to a ESP32-C3 dev module and it is acting as Bluetooth Beacon, advertising x:y coordinates. The loacation data is recieved by Raspberry Pi. Code for ESP32-C3 is in Sender and for Pi in Reciever directory.

## Getting started

The beacon part is written for ESP-IDF.

The recieving part should run with any Python 3 interpretator. Only one pacage, Bleak [https://pypi.org/project/bleak/](https://pypi.org/project/bleak/), needs to be installed.
