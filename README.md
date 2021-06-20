# Sensirion SPS30 connected to Raspberry Pi

## ===========================================================

Software to connect an SPS30 with a Raspberry Pi running I2C. It is able to instruct,
read and display data from an SPS30. The monitor can optionally be
extended to include a DYLOS DC1700 and /or SDS011 monitor and provide common output.

<br> A detailed description of the options and findings are in SPS30_on_raspberry.odt

## Getting Started
This project is part of a number of projects to measure the air quality.
I bought a new sensor : SPS30 from Sensirion.
Having worked on another sensor from them (SCD30) before the quality is normally good.
The aim is to compare the output with other sensors and provide the learning.

The board is relative new at the time of writing this documentation and
as such a lot of preliminary documents from supplier.

I have already written a real working driver for Arduino/ESP32,
but wanted to connect to a Raspberry PI over I2C.

The monitor can optionally be extended to include a DYLOS DC1700
and /or SDS011 monitor and provide common output.

## Prerequisites
BCM2835 library (http://www.airspayce.com/mikem/bcm2835/)

## Software installation
Install latest from BCM2835 from : http://www.airspayce.com/mikem/bcm2835/

1. wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.56.tar.gz
2. tar -zxf bcm2835-1.56.tar.gz     // 1.56 was version number at the time of writing
3. cd bcm2835-1.56
4. ./configure
5. sudo make check
6. sudo make install

To compile the program, go in the directory and type
    make

To create a build the SPS30 with Dylos monitor (in case you have one):
    make BUILD=DYLOS

To create a build the SPS30 with SDS011 monitor (in case you have one):
    make BUILD=SDS011

To create a build the SPS30 with BOTH SDS011 & Dylos monitor:
    make BUILD=BOTH

## Program usage
### Program options
type ./sds330 -h or see the detailed document

## Versioning

### version 1.0 / May 2019
 * Initial version for Raspberry Pi

### version 1.4  / April 2020
 * Based on the new SPS30 datasheet (March 2020) a number of functions are added or updated. Some are depending on the new firmware.
 * Added sleep() and wakeup(). Requires firmware 2.0
 * Added GetVersion() to obtain the current firmware / hardware info
 * Added GetStatusReg() to obtain SPS30 status information. Requires firmware 2.2
 * Added structure SPS30_version for GetVersion
 * Added FWcheck function to check on correct firmware level
 * Added INCLUDE_FWCHECK in SPS30lib.h to enable /disable check.
 * Changed probe() to obtain firmware levels instead of serial number.
 * Changed on how to obtaining product-type
 * Depreciated GetArticleCode() Still supporting backward compatibility
 * Update to documentation

### Version 1.4.4 / July 2020
 * Validated against an SPS30 with firmware 2.2 (thank you Sensirion)
 * Update to documentation

## Author
 * Paul van Haastrecht (paulvha@hotmail.com)

## License
This project is licensed under the GNU GENERAL PUBLIC LICENSE 3.0

## Acknowledgements
### Make sure to read the datasheet from Sensirion. The latest version is March 2020 it does provide good starting point.<br>

### June 2021, Input from CCDZAPPER:

This is not an issue - just a note to help others that may have damaged or lost the flimsy cable with the ZHR-5 connector. It is available inexpensively on eBay: ![https://www.ebay.com/itm/114551266422](https://www.ebay.com/itm/114551266422)
Make sure you pick the correct type, which is 1.25mm 5 Pin.
(10 Sets JST SH 1.0 ZH 1.5 PH 2.0 XH 2.5 Housing Connector Female Male Wire)

