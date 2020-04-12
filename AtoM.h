/********************************************************
 * AtoM.h
 * table of ASCII to Morse code 
 * wb8nbs@gmail.com
 * May 2014
 * 
 * Code table from Wikipedia page on Morse Code
 * Also see discussion and example code at:
 * http://raronoff.wordpress.com/2010/12/16/morse-endecoder/
 * 
 * Prosigns not implemented. Table should allow up to 7 elements
 * but the sketch is written for 6 elements max. 
 * For a prosign, enter upper case first letter and chars run together.
 *  
 * puncuation test paste this in and send: .,?'!/()&:;=+-_"$@
 * (table does not include Underscore)
 * underscore is out of range of this table
 *
 * Logic notes:
 * 1 = Dit, 0 = Dah, rightmost 1 is a stop bit.
 * bits are read out of position 6 then the byte
 * is shifted left and masked until it equals B01000000pgmspa
 * 
 * Unsupported characters are stored as a morse space "0x40"
 *******************************************************/

#include <avr/pgmspace.h>

const unsigned char AtoM[59] PROGMEM = {
  0x40, /* B01000000 Ascii 32   */
  0x29, /* B00101001 Ascii 33 ! */
  0x5B, /* B01011011 Ascii 34 " */
  0x40, /* B01000000 Ascii 35 # */
  0x40, /* B01000000 Ascii 36 $ */
  0x40, /* B01000000 Ascii 37 % */
  0x5E, /* B01011110 Ascii 38 & */
  0x43, /* B01000011 Ascii 39 ' */
  0x26, /* B00100110 Ascii 40 ( */
  0x25, /* B00100101 Ascii 41 ) */

  0x40, /* B01000000 Ascii 42 * */
  0x56, /* B01010110 Ascii 43 + */
  0x19, /* B00011001 Ascii 44 , */
  0x3D, /* B00111101 Ascii 45 - */
  0x55, /* B01010101 Ascii 46 . */
  0x36, /* B00110110 Ascii 47 / */
  0x02, /* B00000010 Ascii 48 0 */
  0x42, /* B01000010 Ascii 49 1 */
  0x62, /* B01100010 Ascii 50 2 */
  0x72, /* B01110010 Ascii 51 3 */

  0x7A, /* B01111010 Ascii 52 4 */
  0x7E, /* B01111110 Ascii 53 5 */
  0x3E, /* B00111110 Ascii 54 6 */
  0x1E, /* B00011110 Ascii 55 7 */
  0x0E, /* B00001110 Ascii 56 8 */
  0x06, /* B00000110 Ascii 57 9 */
  0x0F, /* B00001111 Ascii 58 : */
  0x2B, /* B00101011 Ascii 59 ; */
  0x40, /* B01000000 Ascii 60 < */
  0x3A, /* B00111010 Ascii 61 = */

  0x40, /* B01000000 Ascii 62 > */
  0x67, /* B01100111 Ascii 63 ? */
  0x4B, /* B01001011 Ascii 64 @ */
  0x50, /* B01010000 Ascii 65 A */
  0x3C, /* B00111100 Ascii 66 B */
  0x2C, /* B00101100 Ascii 67 C */
  0x38, /* B00111000 Ascii 68 D */
  0x60, /* B01100000 Ascii 69 E */
  0x6C, /* B01101100 Ascii 70 F */
  0x18, /* B00011000 Ascii 71 G */

  0x7C, /* B01111100 Ascii 72 H */
  0x70, /* B01110000 Ascii 73 I */
  0x44, /* B01000100 Ascii 74 J */
  0x28, /* B00101000 Ascii 75 K */
  0x5C, /* B01011100 Ascii 76 L */
  0x10, /* B00010000 Ascii 77 M */
  0x30, /* B00110000 Ascii 78 N */
  0x08, /* B00001000 Ascii 79 O */
  0x4C, /* B01001100 Ascii 82 P */
  0x14, /* B00010100 Ascii 81 Q */

  0x58, /* B01011000 Ascii 82 R */
  0x78, /* B01111000 Ascii 83 S */
  0x20, /* B00100000 Ascii 84 T */
  0x68, /* B01101000 Ascii 85 U */
  0x74, /* B01110100 Ascii 86 V */
  0x48, /* B01001000 Ascii 87 W */
  0x34, /* B00110100 Ascii 88 X */
  0x24, /* B00100100 Ascii 89 Y */
  0x1C, /* B00011100 Ascii 90 Z */ 
};
