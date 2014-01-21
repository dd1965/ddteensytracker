ddteensytracker
===============
C/C++/INO code for Teensy3.1. It transmits fast scan SSDV (at AFSK 1200 baud , 4800 baud raised cosine pulse 9600 baud raised cosine pulse) from a Serial Camera and  telemetry information from a GPS into a transmitter module. It also includes a CW transmitter with grid locator via a DDS 9850 on 30m and ability to send AX25 APRS packets on a round robin basis.

Most of the hardwork has been done by Phil Karn (Reed Solomon encoder) and Philip Heron (SSDV breaking up of JPEG files for tranmission and reassembly).

My work has been mainly to get the comms quicker and more reliable on the telemetry and SSDV by using correlation tags at the begining of the packet so that start of frame can be detected and then the reed solomon encoder/decoder can do the hard work of keeping the transmission reliable.

Since it is a mixed source of files, there have been a few Kludges done to get it to compile under the arduino IDE. This is running on a Teensy3.1. Modifications have been done to some of the core compiler library of the teensy to get this to work correctly for the Teensy3.1 If you want details, contact me. The code does all work and we have done some distance testing transmitting the AFSK type mod schemes via a normal FM 433Mhz channel. The error correction is quite strong and the detection of start of frame using gold codes helps a lot. Some refactoring is still needed, but its all WIP.

Main changes are
Dallas temp library modified to be more reliable.
Timer lib modified to accept a sample rate as the interrupt timing.
HW Serial buffer extended to 256bytes (Note: the serial lib is still not as reliable as the arduino due lib, but its quite good now)
Some kludges were also necessary to get the SPI to work on the correct pin.
Note printf does not work on the Teensy3.1 and hangs the device.
Cheers /DD VK3TBC
