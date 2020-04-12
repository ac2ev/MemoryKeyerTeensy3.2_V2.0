/********************************************************
 * Canned.h
 * Seven Default canned messages for the arduino memory keyer
 * Each can be up to MSGSIZE-1 characters. Also other defaults that
 * might be changed by a user.
 * wb8nbs@gmail.com
 * March 2016
 *******************************************************/

#include <avr/pgmspace.h>

// the following sets up defaults for the four memory messages.

// Each can be MSGSIZE characters, the MSGSIZE+1 char is the strings trailing null.
// Size is subject to available EEPROM memory, 2k on the Teensy3.2
// Normally all lower case, upper case letters will not sound an inter-character space
// so you can build prosigns, for instance 'As' will run together dit dah dit dit dit
const int MSGSIZE = 51;              // Max length of memory message
const unsigned char cMessg[7][MSGSIZE] = {
  "cq cq cq de ac2ev ac2ev k",                          // Message 0, button 1
  "ac2ev",                          // Message 1, button 2
  "tu 5NN",                          // Message 2, button 3
  "fn13dg",                          // Message 3, button 4
  "mem 5 ",                          // Message 4, button 5
  "mem 6 ",                          // Message 5, button 6
  "test test de ac2ev ac2ev"                           // Message 6, button 7
};

const int MINLCD = 12;               // Dimmest LCD backlight
const int MAXLCD = 250;              // Brightest LCD backlight
const int DEFAULTLCD = 125;          // Default LCD backlight

const int MAXTONE = 1200;            // highest allowed sidetone freq
const int MINTONE = 200;             // lowest allowed sidetone freq
const unsigned int MIDTONE = 320;    // middle tone announce frequency
const unsigned int LOWTONE = 440;    // low tone announce frequency
const unsigned int DEFAULTTONE = 600;// side tone frequency

const int MAXWPM = 45;               // maximum WPM speed allowed
const int MINWPM = 10;               // minimum WPM speed allowed
const int DEFAULTSPEED = 17;         // default Words Per Minute

const int GMTOFFSET = -6;            // Default local ltime offset from GMT
const int LONGPRESS = 2000;          // Two seconds in millis

const int PRACPAUSE = 3;             // # spaces between 5 char practice groups

// These two constants tune the paddle decode routine.
// # idle element periods (-1) before a character space is declared
const int CSPACEWAIT = 2;
// # idle element periods (-1) before a word space is declared
const int WSPACEWAIT = 7;

const int MAXLINE = 72;              // # chars before a linefeed on serial port

const int MAXQ = 64;                 // sets size of transmit queue buffer
const int LOWTRIGGER = 341;          // Low batt voltage trigger level (hundredths)

/* Other useful information:

  PS2 keyboard library from http://playground.arduino.cc/Main/PS2Keyboard
  but this version should be using equivalent Teensy library from PJRC.
  atmega328 notes (uno, duemilanova, et al.) from the above web page:
  data pin can be any available digital but clock must be D2 or D3 for the interrupt to work.
  PS2 six pin mini DIN connector is wired pin 1 data, pin 3 ground, pin 5 clock, pin 4 +5 volts
  Looking at face of *Female* PS2 Connector:
      6  k  5
         e
     4   y    3

       2    1


 Analog button switch hookup:
  One side has 10k resistor to +5 and is also connected to Arduino analog pin. Other side
  connected to ground. Connect a 0.01 microfarad cap across the switch.
  Note any unterminated Analog inputs float to about half scale and will hang the sketch.
 */
