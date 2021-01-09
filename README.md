# EV1527Link
An EV1527 gateway for use with Domoticz or other home automation systems that support RF Link protocol.

# Download
https://github.com/DarenFord/EV1527Link/releases/latest

# Description




# Hardware
A Fritzing schematic diagram is included in the package and is extemely simple to wire-up. Build it on a experimental breadboard first to test that it works and when you're happy, solder the components onto prototype PCB. If you're feeling really adventurous, you could create a PCB layout and add it to the project :-)

It is important to use a good quality receiver module. The receiver module that sometimes comes with the FS-1000A transmitter module is not good enough and I would suggest using an RXB6 module (see parts list below) or something of similar quality.

The parts required to build the gateway:
- 1x Arduino Nano (or other Arduino with a USB port)
- 1x A good 433MHz receiver and ASK transmitter module (e.g. RXB6 receiver and transmitter set https://www.aliexpress.com/item/1005001504888006.html)
- 2x 1K resistor
- 1x Red LED
- 1x Green LED
- 1x Push button for testing

# Installation



# Configuration in Domoticz


# LAN version
It is possible to connect the gateway directly to the local network and access it from Domoticz. To do this, add a serial to TCP converter (e.g. https://www.aliexpress.com/item/32329316229.html), connected to the TX & RX pins of the Arduino. Don't forget to level-shift the 5 volt TX from the Arduino to the 3 volt RX pin of the serial converter module and add jumpers to your circuit board, so that the converter can be disconnected if and when you need to program the Arduino.

# Contribute to EV1527Link
If you want to contribute: Just create a pull request with your bugfix, addition or anything else you think is valuable.

Something that would be useful is a PCB layout in a format that is supported by PCB manufacturers.

Thanks for your help!