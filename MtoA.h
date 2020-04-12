/********************************************************
 * MtoA.h
 * wb8nbs@gmail.com
 * November 2014
 * 
 * Table of Ascii eqivalent of morse characters. 
 * An index into this table is formed by assigning 1 to dit, 0 to dahs. Those
 * bits are ored into index byte right to left starting with the LSB. Finally 
 * a stop bit is added at the higher bit position.
 *******************************************************/
 
#include <avr/pgmspace.h>

//static const prog_char MtoA[128] PROGMEM = {
const unsigned char MtoA[128] PROGMEM = {

  ' ',  // 0x00
  '~',  // 0x01 
  'T',  // 0x02
  'E',  // 0x03
  'M',  // 0x04
  'A',  // 0x05
  'N',  // 0x06
  'I',  // 0x07
  'O',  // 0x08
  'W',  // 0x09
  'K',  // 0x0A
  'U',  // 0x0B
  'G',  // 0x0C
  'R',  // 0x0D
  'D',  // 0x0E
  'S',  // 0x0F
  'x',  // 0x10
  'J',  // 0x11
  'Y',  // 0x12
  'x',  // 0x13
  'Q',  // 0x14
  'x',  // 0x15
  'X',  // 0x16
  'V',  // 0x17
  'x',  // 0x18
  'P',  // 0x19
  'C',  // 0x1A
  'F',  // 0x1B
  'Z',  // 0x1C
  'L',  // 0x1D
  'B',  // 0x1E
  'H',  // 0x1F
  '0',  // 0x20
  '1',  // 0x21
  'x',  // 0x22
  '2',  // 0x23
  'x',  // 0x24
  'x',  // 0x25
  'x',  // 0x26
  '3',  // 0x27
  'x',  // 0x28
  'x',  // 0x29
  'x',  // 0x2a
  'x',  // 0x2b
  'x',  // 0x2c
  'x',  // 0x2d
  '=',  // 0x2E
  '4',  // 0x2F
  '9',  // 0x30
  'x',  // 0x31
  '(',  // 0x32
  'x',  // 0x33
  'x',  // 0x34
  '+',  // 0x35
  '/',  // 0x36
  'x',  // 0x37
  '8',  // 0x38
  'x',  // 0x39
  'x',  // 0x3a
  'x',  // 0x3b
  '7',  // 0x3C
  '&',  // 0x3D
  '6',  // 0x3E
  '5',  // 0x3F
  'x',  // 0x40
  'x',  // 0x41
  'x',  // 0x42
  'x',  // 0x43
  'x',  // 0x44
  'x',  // 0x45
  'x',  // 0x46
  'x',  // 0x47
  'x',  // 0x48
  'x',  // 0x49
  '!',  // 0x4A
  'x',  // 0x4b
  ',',  // 0x4c
  'x',  // 0x4d
  'x',  // 0x4e
  'x',  // 0x4f
  'x',  // 0x50
  'x',  // 0x51
  ')',  // 0x52
  '_',  // 0x53
  'x',  // 0x54
  '.',  // 0x55
  'x',  // 0x56
  'x',  // 0x57
  'x',  // 0x58
  'x',  // 0x59
  'x',  // 0x5a
  'x',  // 0x5b
  'x',  // 0x5c
  'x',  // 0x5d
  '-',  // 0x5E
  'x',  // 0x5f
  'x',  // 0x60
  '\'',  // 0x61
  'x',  // 0x62
  'x',  // 0x63
  'x',  // 0x64
  'x',  // 0x65
  'x',  // 0x66
  'x',  // 0x67
  'x',  // 0x68
  '@',  // 0x69
  ';',  // 0x6A
  'x',  // 0x6b
  'x',  // 0x6c
  '\"',  // 0x6D
  'x',  // 0x6e
  'x',  // 0x6f
  'x',  // 0x70
  'x',  // 0x71
  'x',  // 0x72
  '?',  // 0x73
  'x',  // 0x74
  'x',  // 0x75
  'x',  // 0x76
  'x',  // 0x77
  ':',  // 0x78
  'x',  // 0x79
  'x',  // 0x7a
  'x',  // 0x7b
  'x',  // 0x7c
  'x',  // 0x7d
  'x',  // 0x7e
  'x'   // 0x7f
};
