/********************************************************
   MemoryKeyerTeensy3.2_V1.0.ino compiled with Arduino 1.6.7
   Arduino iambic CW memory keyer for ham radio usage
   This version is a port to the PJRC Teensy-3.2 Cortex-M4.

   Links to the latest code version will be at the end of:
   https://wb8nbs.wordpress.com/2016/02/11/arduino-iambic-keyer-2016-part-1-hardware/
   https://wb8nbs.wordpress.com/2016/02/10/arduino-iambic-keyer-2016-part-2-software/
   https://wb8nbs.wordpress.com/2016/02/09/arduino-iambic-keyer-2016-part-3-operation/
   Jim Harvey mailto:wb8nbs@gmail.com

   Many thanks to KC4IFB, his iambic code was the starting point for this
   keyer project. His code feels exactly like a WB4VVF Accukeyer.
   Richard Chapman KC4IFB "Build a Low-Cost Iambic Keyer Using Open-Source Hardware"
   QEX - September 2009 - code available on the ARRL web site.

   Circular Queue implemented with help from examples at:
   http://embedjournal.com/implementing-circular-buffer-embedded-c/

   Modification log:
   Jan 2014 Mod to eliminate speed pot. Close both paddles for at least three
     seconds, beeps, will go into speed adjust mode; dot increases speed, dash decreases.
     Again close both paddles 3 secs, beeps, exits speed adjust mode. (note: removed)
   Jan 2014 Add Straight Key mode. Enabled if Dash (ring) pin is closed
            at power on or reset.
   Jan 2014 Add toneOn flag so it doesnt set tone on every iteration. tiny85 doesnt like that.
   May 2014 Add startup routine to beep out a boot message. Note: removed in Dec.
   May 2014 Changed morse table method, add serial to morse capability, fixed messages in flash
   May 2014 Added stored (by compiler) messages and keyboard speed change
   Nov 2014 Add PS2 support, remove serial. Reset with both paddles closed will set 13 WPM.
   Dec 2014 move memories to eeprom so they can be programable from keyboard.
            Added a function button. Add serial back in, will send from either kbd.
            Change paddle speed set. Add restore default to eeprom on reset, Add memory program.
            Changed morse table again to smaller tree derived table after discussion at
            http://raronoff.wordpress.com/2010/12/16/morse-endecoder/
            Added morse to ascii table so ascii memories can be programmed from paddles
            Added four morse practice modes. Built a box to put it in.
   Jan 2015 Cleanup doIambickey
   Feb 2015 V2.0 Significant changes converting to fully asynchronous event loop
            All delay() and spin waits removed from the main loop path.
            Remove numbers only practice. Change eeprom order. Add change sidetone.
            V2.01 minor tuning space generation in doIambic.
            V2.02 Converted sidetone generation to DDS/PWM sine wave with help from
            http://ko7m.blogspot.com/2014/08/direct-digital-synthesis-dds.html
            http://www.analog.com/media/en/training-seminars/tutorials/MT-085.pdf
            V2.03 changed sine table to half step offset. Saves a byte in the table.
            V2.04 added crude wave shaping to front and rear of keyed sidetone. Method used
            is timed switching between three amplitude tables. Had to debounce straight key.
            V2.0.4.1 Minor bug fix to disable interrupt while changing sine tables
   Mar 2015 V2.0.5 change AtoM table from int to char saves 68 bytes but created setup bug
            initializing digital pins. Change do loop to i<19, i<21 was killing tone.
            Fix bug in initByte, aaByte = aByte -= 0x20; did not work in 1.6.1 compile.
   Dec 2015 Port to 32 bit TeensyLC using DAC for sine synthesis
            LC has only 128 bytes of EEPROM so messages are limited to 30 char.
   Jan 2016 Begin port to Teensy3.2 with 7 messages, LCD, and Real Time Clock
   Feb 2016 V1.0.0 Hardware is done. Work on software additions clock, menus
   Mar 2016 V1.1.0 CPM logic bug sometimes allowing memory button insertion
            Redo low battery alarm, fix bugs found in testing
   Mar 2016 V1.1.1 Display fix - LCD does not have back slash in its font
   Mar 2020 V2.0.0 USB HID Keyboard, I2C Liquid Crystal -pin re-assignments AC2EV
 ********************************************************/
const char Version[] = "Version 2.0.1"; // Keep this current

#include <EEPROM.h>             // needed to save memories and speed setting
//SCL1 - 29 SDA1 - 30
#include <i2c_t3.h> 
#include <LiquidCrystal_I2C_T3.h>

//#include <LiquidCrystalFast.h>  // ucomment Teensy LCD

#include <PS2Keyboard.h>        // Will load Teensy version

#include <TimeLib.h>            // Teensy Real Time Clock support
#include "AtoM.h"               // ASCII to Morse translation table
#include "MtoA.h"               // Morse to ASCII translation table
#include "Canned.h"             // Defaults: messages and frequently changed parms

//LiquidCrystalFast lcd(0, 1, 2, 7, 8, 9, 10); //uncomment for Teensy LCD
LiquidCrystal_I2C_T3 lcd(0x27, 16, 2,LCD_5x8DOTS, 29, 30);
unsigned int lcdChars;          // Counts LCD characters emitted

// Sine wave amplitude lookup tables length 64, range is zero to 255
// 64 samples per period but using quarter wave symmetry, table is only 16 bytes
// three tables are used so wave amplitude can start and stop smoother
// tables are switched by changing pointers on the fly
// note: half step offset values used to get around a symmetry glitch

const unsigned int bitMask64 = 0x3f; // # of bits in 64 step table index

const unsigned char sineTable1_0[16] = {
  // 64 step full amplitude table
  133, 146, 158, 170, 181, 192, 203, 212, 221, 229, 236, 242, 247, 250, 253, 254
};
// Three quarter amplitude table
const unsigned char sineTable3_4[16] = {
  132, 141, 150, 159, 168, 176, 184, 191, 197, 203, 208, 213, 216, 219, 221, 222
};
// Half amplitude table
const unsigned char sineTable1_2[16] = {
  130, 136, 142, 148, 154, 159, 165, 169, 174, 178, 181, 184, 186, 188, 189, 190
};

// These must all be marked as volatile as they are used
// in the sine synthesizer interrupt service routine
unsigned char volatile const *sineTable = sineTable1_2; // ISR pointer to sine table

IntervalTimer waveTimer;        // Create a Teensy interval timer for sine generator

// PS2 Pin definitions
const int DATAPIN = 10;          // Defined keyboard data pin
const int IRQPIN =  9;          // Defined keyboard clock pin
PS2Keyboard keyboard;           // Invoke the PS2 library

const int BACKLITE = 5;         // Pin to control LCD backlight

const int DOTIN = 11;           // LOW -> dot paddle closed - Should be jack TIP
const int DASHIN = 12;          // LOW -> dash paddle closed - Should be jack RING
// don't use pin 13 for keyout it gets hit several times during boot up.
const int KEYOUT = 6;           // Drives the transmitter key connection
// On TeensyLC the DAC is on A12, Teensy3.2 on A14
const int TONE = A14;           // Pin to output side tone audio from DAC

const int firstButton = A1;     // First of 7 sequential analog pins for buttons
const int lastButton = firstButton + 6; // Last of the 7 sequential buttons
const int FUNCTION = A0;        // Function button

// define four possible states of the iambic machine
const int IDLE = 0;             // Doing nothing
const int DASH = 1;             // Playing a dash
const int DOT = 2;              // Playing a dot
const int DELAY = 3;            // In the dot-length delay between two dot/dashes

int dotLength;                  // Length of a dot, the basic unit of timing
boolean dotVal;                 // Value of the dot paddle this cycle of main loop
boolean dashVal;                // Value of the dash paddle this cycle of main loop
int currElt = IDLE;             // State of what the keyer output is sending right now
int nextElt = IDLE;             // State the keyer will go into when current element ends
int lastElt = IDLE;             // Previous state of the keyer
unsigned long Mtime;            // Used to store a millis() reading
unsigned long currEltendtime;   // What time should current element finish (millis)
unsigned long T0, T1, T2;       // Used to switch waveshape tables

int sideTone;                   // Current side tone frequency
int tablePeriod;                // Microsec between samples
byte waveCounter;               // Steps thru entire sine wave

boolean inTone = false;         // Records if in side tone set mode
boolean lastTone = false;       // Records if side tone mode was exited
boolean rampUp = false;         // Set up sloped beginning of side tone
boolean rampDown = false;       // Set up sloped ending of side tone

int wordsPerminute;             // Speed setting in words per minute
boolean inSpeed = false;        // Records if in speed set mode
boolean lastSpeed = false;      // Used to determine if speedmode has exited

boolean toneOn = false;         // Remembers that tone was turned on
boolean skMode = true;          // Flag is unset if in Straight Key mode
boolean keyTherig = true;       // False if NOT keying rig (program, practice)
boolean keyTherigQ = true;      // False if NOT keying rig until Q is empty
boolean commandMode = false;    // Set if in keyboard command mode

char asciiChar;                 // Morse converted to ascii for printing
boolean enteringIdle =  true;   // Records that first idle time was recorded
int inByte;                     // Char input from either keyboard or paddles
boolean gotCharkey = false;     // True if char is waiting to process from keyboards
boolean gotCharpad = false;     // True if char is waiting to process from paddles
int charsOut = 0;               // Counts charcters written to serial port
char aByte = 0;                 // ASCII retrived from Queue or message memory

int CPM = 0;                    // Which button triggered code practice mode
int farnsWorthiness = 0;        // Amount of extra element Farnsworth delay
bool skipInput = true;          // Skip char input checks if false

int sendQ[MAXQ];                // Transmit character circular message buffer
unsigned int head = 0;          // Next position to insert into Queue
unsigned int tail = 0;          // Next position to retrieve from Queue

time_t Local;                    // Local time
time_t GMT;                      // Greenwich Mean Time
#define TIME_HEADER  "T"         // Header tag for serial time sync message
int GMTOffset;                   // Hours offset GMT to local in eeprom

unsigned long nextMenu = 0;      // Time in millis to switch menus

boolean lowBattFlag = false;     // Backlight flasher
const int batVoltsPin = A10;     // Battery voltage monitor
const int lowBattPin = 23;       // Low battery alarm/const
const int usbPwrPin = 13;        // +5 from USB
const int chgStatPin = 22;       // Batt charge status

int lcdBackLite;                 // LDD brightness, init from eeprom

const int ENTER = A5;            // Button to enter a setup menu
const int UP = A6;               // Button for increasing
const int DOWN = A7;             // Button for decreasing

int batVolts;                    // Battery voltage
int batVoltsSmoothed;            // Voltage averaged
int batVoltsAtLow = 0;
unsigned long updateVoltTime;    // Millis target next voltage read


/********************************************************
   Arduino setup
 ********************************************************/
void setup() {
  int whichMenu;                 // Will select one of the setup menus

  // Fire up the liquid Crystal display
  pinMode(BACKLITE, OUTPUT);     // Pin to control LCD backlight
  analogWrite(BACKLITE, DEFAULTLCD); // To get started
  lcd.begin();
  lcd.clear();

  Serial.begin(115200);          // Set up for serial keyboard input and output
  keyboard.begin(DATAPIN, IRQPIN); // Initialize the PS2 keyboard
  // delay(1500);                // Some PCs need this

  // Keyout pin drives an Opto coupler and the transmit LED
  // Nine milliamp max on a Teensy output pin
  pinMode(KEYOUT, OUTPUT);       // Set mode on the trnsmit i/o pin
  digitalWrite(KEYOUT, LOW);     // Initally the key output is open (low)

  // NOTE paddle pins idle HIGH, goes LOW (grounded) when paddle is closed.
  pinMode(DOTIN, INPUT);
  pinMode(DASHIN, INPUT);
  digitalWrite(DOTIN, HIGH);     // activate built in pull up
  digitalWrite(DASHIN, HIGH);    // activate built in pull up

  // EEPROM memory organization:
  //   0,1 int sidetone frequency
  //   2   byte WPM speed
  //   3   byte Liquid Crystal Back light level
  //   4,5,6,7 signed int GMT offset from Local time
  //   8 - end  Copy of the seven canned messages MSGSIZE bytes each

  // Retrieve last saved sideTone frequency setting
  sideTone = EEPROM.read(0) | (EEPROM.read(1) << 8);   // Retrieve saved speed setting

  // Retrieve last saved speed setting
  wordsPerminute = (int)EEPROM.read(2); // Retrieve last saved speed setting
  dotLength = 1200 / wordsPerminute;    // Initialize the element length

  // Retrieve saved back light setting
  lcdBackLite = (int)EEPROM.read(3);
  analogWrite(BACKLITE, lcdBackLite); // Init from EEProm

  // Retrieve saved GMT offset 32 bit signed int
  GMTOffset = EEPROM.read(4) | (EEPROM.read(5) << 8) | (EEPROM.read(6) << 16) | (EEPROM.read(7) << 24);

  // Memory messages are always read directly from EEPROM when needed

  // Battery monitor stuff
  analogReadResolution(12);      // Teensy 3.2 range 0 - 4095
  analogReference(DEFAULT);      // Ensure internal reference
  pinMode(batVoltsPin, INPUT);   // Calibrated battery voltage
  pinMode(usbPwrPin, INPUT);     // USB power status from charger
  pinMode(chgStatPin, INPUT);    // Charging status from charger
  pinMode(lowBattPin, INPUT);    // Low battery alarm from charger

  // Real Time Clock stuff
  // Set the Time library to use Teensy 3.2's RTC to keep time
  setSyncProvider(getTeensy3Time);

  lcd.setCursor(0, 0);
  // From the Teensy Time library examples:
  if (timeStatus() != timeSet) {
    Serial.println("Unable to sync with the RTC");
    lcd.print("Can't sync RTC");
  } else {
    Serial.print("RTC has set the system time to ");
    GMT = now();                 // Dig out what was set
    digTimeDispSerial(GMT, 1);
    Serial.print(" ");
    digDateDispSerial(GMT, 4);
    Serial.println(" GMT");

    lcd.print("RTC Synchronized");
    lcd.setCursor(0, 1);
    digTimeDispLCD(GMT, 1);
  }
  delay(1000);                   // Allows user to read message
  lcd.clear();

  // Check for request to enter Code Practice Mode
  for (int i = firstButton; i <= firstButton + 2; i++) { // scan the memory buttons
    if (!Button(i)) {
      CPM = i;                   // Mark to go into code practice
      beeps(LOWTONE);            // Announce that we caught a button
    }
    while (!Button(i)) {}        // Spin wait for button to be released
    randomSeed(millis());        // Add entropy
  }

  if (CPM != 0) {                // We did catch a CPM request
    Serial.print("Practice Mode ");
    lcd.setCursor(0, 0);
    lcd.print("Practice Mode ");
    Serial.print(CPM - firstButton + 1);
    Serial.print(": ");
    lcd.setCursor(0, 1);

    switch (CPM - firstButton) {
      case 0:
        Serial.print("Letters Only");
        lcd.print("Letters Only ");
        break;
      case 1:
        Serial.print("Letters and Numbers");
        lcd.print("Letters Numbers ");
        break;
      case 2:
        Serial.print(": Letters, Numbers, and Punctuation");
        lcd.print("Lett, Numb, Punc");
        break;
    }

    Serial.print(" at ");
    Serial.print(wordsPerminute);
    Serial.println(" WPM");
    skipInput = false;           // Flag to skip character inputs
  }

  // Settings menu event loop; invoked if Func held down at bootup
  if (!Button(FUNCTION)) {       // Func button, enter setup menu mode

    whichMenu = 7;               // Force menu to start at beginning
    nextMenu = 0;                // Initialize menu rotation time
    lcd.setCursor(0, 0);
    lcd.print("Enter Setup Mode");
    while (!Button(FUNCTION)) {} // Spin wait for Func to release

    // Option to synchronize Teensy RTC from Serial - see TimeTeensy3 example
    // Following code will accept a Unix time_t from serial
    // Must send in "T" plus time_t while "Enter Setup Mode" is displayed
    // THEN release FUNCTION to start the menus
    if (Serial.available()) {
      GMT = processSyncMessage();

      if (GMT != 0) {
        Teensy3Clock.set(GMT);   // Set the RTC
        setTime(GMT);

        lcd.setCursor(0, 0);
        lcd.print("RTC Synchronized");
        Serial.println("\nRTC Synchronized");
      } else {
        lcd.setCursor(0, 0);
        lcd.print("RTC Synch Fail  ");
        Serial.println("\nRTC Synchronization Fail");
      }
      lcd.setCursor(0, 1);
      digTimeDispLCD(GMT, 1);
      delay(1500);               // Pause to view message
    }

    // Setup menu mode event loop. Will loop until Func is pressed again.
    while (true) {
      Mtime = millis();

      // Keep a continuous running average of the battery voltage
      if (Mtime > updateVoltTime) {
        readBatt();
        updateVoltTime = Mtime + 505L; // Update 2 times per second
      }

      if (Mtime > nextMenu) {    // Time to rotate next menu item
        nextMenu = Mtime + 1500L; // Next rotation time
        whichMenu = (whichMenu + 1) % 8; // Next item in menu

        lcd.setCursor(0, 0);
        lcd.print("Setup Mode      ");
        lcd.setCursor(0, 1);

        switch (whichMenu) {     // Display menu item
          case 0:
            lcd.print("   Speed WPM    ");
            lcd.setCursor(0, 1);
            lcd.print(wordsPerminute);
            break;
          case 1:
            lcd.print("     Tone Freq  ");
            lcd.setCursor(0, 1);
            lcdPz(sideTone, 4);
            break;
          case 2:
            lcd.print("    Backlight   ");
            lcd.setCursor(0, 1);
            lcd.print(lcdBackLite);
            break;
          case 3:
            GMT = now();
            lcd.print("      GMT Time  ");
            lcd.setCursor(0, 1);
            digTimeDispLCD(GMT, 0);
            break;
          case 4:
            GMT = now();
            lcd.print("           Date ");
            lcd.setCursor(0, 1);
            digDateDispLCD(GMT, 4);
            break;
          case 5:
            lcd.print("    GMT Offset  ");
            lcd.setCursor(0, 1);
            if (GMTOffset >= 0) lcd.print("+");
            lcd.print(GMTOffset);
            break;
          case 6:
            lcd.print("Reset to Default");
            break;
          case 7:
            lcd.setCursor(0, 0);
            lcd.print("Battery Status  ");
            lcd.setCursor(0, 1);
            digTimeDispSerial(GMT, 1);
            Serial.print(" ");
            digDateDispSerial(GMT, 4);
            Serial.print(" ");
            chargeStatus();      // Status to both LCD and Serial
            break;
        }                        // End display switch
      }                          // End rotate menu

      if (!Button(ENTER)) doSetupMenu(whichMenu); // DO something!

      // Test for setup menu exit request
      if (!Button(FUNCTION)) {   // Function button, exit setup menu
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Write to EEPROM ");
        sidetoneToeeprom();      // Write side tone frequency
        wpmToeeprom();           // Write WPM speed to eeprom
        EEPROM.write(3, (byte)lcdBackLite);// Back Light level
        // GMTOffset is a signed number so need all 32 bits.
        EEPROM.write(4, (byte)(GMTOffset & 0xFF));
        EEPROM.write(5, (byte)(GMTOffset >> 8) & 0xFF);
        EEPROM.write(6, (byte)(GMTOffset >> 16) & 0xFF);
        EEPROM.write(7, (byte)(GMTOffset >> 24) & 0xFF);
        // Note: the remainder of EEPROM can be used as message memory
        lcd.clear();

        while (!Button(FUNCTION)) {} // Spin wait for Func to release
        break;                   // Exit
      }                          // End of setup menu exit check
    }                            // End menu rotation
  }                              // End setup menu mode

  // Set flag to enter Straight Key mode if only dash pin is LOW at startup
  // A straight key uses a 2 conductor plug which will ground the ring
  // of the three conductor paddle jack,
  if (!digitalRead(DASHIN)) {
    skMode = false;
    Serial.println("\n");
    Serial.println("Straight Key");
    lcd.setCursor(0, 1);
    lcd.print("Straight Key    ");

    beeps(LOWTONE);              // Announce Straight Key mode
    Mtime = millis() + 500;      // Waste a half second
    while (millis() < Mtime) {   // Spin
    }
  }

  beeps(MIDTONE);                // Announce we are ready for business
  Serial.println(F("\n"));
  Serial.println("Keyer Ready");
  lcd.cursor();

  if (CPM == 0) {                // NOT in practice mode
    lcd.setCursor(0, 0);
    lcd.print(Version);
    lcd.setCursor(0, 1);
    lcd.print("Keyer Ready     ");
  }
}


/********************************************************
   Arduino main loop
 ********************************************************/
void loop() {
  static int pressed;            // Holds pin # of button that was pressed
  static unsigned long nextUpdate = 0L; // Millis() time to refresh
  static int whichStatus = 0;    // Route to a specific status display
  static bool pauseFlag = false; // To pause code practice mode
  static bool lastPause;         // Previous pause state
  static int currentBack = lcdBackLite; // Remembers brightness of LCD back light

  Mtime = millis();              // Time since bootup for this iteration
  waveShape();                   // Manage the sidetone generation
/*
  // Update the continuous running average of the battery voltage
  if (Mtime > updateVoltTime) {
    // To get more precision with integer math, 12 bit ADC readings
    // are calibrated by an external voltage divider pot to 8 times
    // voltage in hundreths of a volt. Connect DVM to battery,
    // adjust pot until display agrees with DVM.
    readBatt();                  // Current battery voltage
    updateVoltTime = Mtime + 501L; // Update voltage 2 times per second
  }

  if (lowBattFlag) {             // Flash LCD if software detected low batt
    if (!((Mtime & 0x0700) != 0)) { // Will be off 1/8 of time
      analogWrite(BACKLITE, MINLCD);
    } else {
      analogWrite(BACKLITE, lcdBackLite);
    }
  } else {                       // Backlight to normal
    analogWrite(BACKLITE, currentBack);
  }
*/

  // Display system variables and status if Function is held down
  // But wait for transmit queue to empty
  if (!Button(FUNCTION) && (aByte == 0)) { // Scroll status if func down
    skipInput = false;           // Disables character input functions
    pauseFlag = true;            // Stops the morse engine
    analogWrite(BACKLITE, lcdBackLite); // Restore original brightness

    lcd.noCursor();              // Disable the cursor

    // Roll the menu display IF neither paddle is closed for speed adj
    if ((Mtime > nextUpdate) && dotVal && dashVal) {
      nextUpdate = Mtime + 1000L; // Display for 1.0 seconds
      nosineTone();              // Turn off any side tone
      if (whichStatus == 0) Serial.println();

      switch (whichStatus) {     // Select an item to display
        case 0:                  // Zulu and Local time
          GMT = now();           // Time from system
          lcd.setCursor(0, 0);   // Greenich Mean Time
          digTimeDispLCD(GMT, 1);
          lcd.print(" ");
          digDateDispLCD(GMT, 0);

          lcd.print(" Z");
          Serial.print("GMT time ");
          digTimeDispSerial(GMT, 1);
          Serial.print("  ");
          digDateDispSerial(GMT, 4);

          Local = GMT + (GMTOffset * 3600);
          lcd.setCursor(0, 1);    // Local time
          digTimeDispLCD(Local, 1);
          lcd.print(" ");
          digDateDispLCD(Local, 0);

          lcd.print(" L");
          Serial.print("    Local time ");
          digTimeDispSerial(Local, 1);
          Serial.print("  ");
          digDateDispSerial(GMT, 4);
          Serial.println();
          break;

        case 1:                  // Operating parameters
          lcd.setCursor(0, 0);   // WPM
          lcd.print("   Words/Minute ");
          lcd.setCursor(0, 0);
          lcd.print(wordsPerminute);
          lcd.setCursor(0, 1);   // Audio sidetone frequency
          lcd.print("     Tone Freq  ");
          lcd.setCursor(0, 1);
          lcd.print(sideTone);

          Serial.print("WPM = ");
          Serial.print(wordsPerminute);
          Serial.print("  Tone ");
          Serial.print(sideTone);
          Serial.println(" Hz");
          break;

        case 2:                  // Battery status
          lcd.setCursor(0, 0);   // Print battery voltage
          lcd.print("Battery Status  ");
          chargeStatus();
          break;

        case 3:                  // Memory messages first 16 chars.
          showMem(0);
          break;
        case 4:                  // Memory messages first 16 chars.
          showMem(1);
          break;
        case 5:                  // Memory messages first 16 chars.
          showMem(2);
          break;
        case 6:                  // Memory messages first 16 chars.
          showMem(3);
          break;
        case 7:                  // Memory messages first 16 chars.
          showMem(4);
          break;
        case 8:                  // Memory messages first 16 chars.
          showMem(5);
          break;
        case 9:                  // Memory messages first 16 chars.
          showMem(6);
          break;
      }                          // End Switch
      // Increment to show the next menu item
      whichStatus = (whichStatus + 1) % 10; // 10 status displays
    }                            // End update time expired check
  }  else {                      // Func button is NOT down

    if (nextUpdate != 0) {       // Clean up after status display
      nextUpdate = 0;            // But only do it once
      lcd.cursor();              // Re-enable cursor
      lcd.setCursor(0, 0);       // Position to first line
      // Note this leaves last status display on the LCD
      lcdChars = 32;             // Wrap LCD chars out counter
      Serial.println();          // New line on serial
      whichStatus = 0;           // So menu starts at beginning
      if (CPM == 0) skipInput = true; // Resume character inputs
      pauseFlag = lastPause;     // Restore the paused state from CPM
    }
  }

  // Check if code practice mode was requested
  //   CPM has pin number of button else zero
  //   aByte is zero when Queue is completely empty

  if (CPM != 0) {                // IF in Code Practice Mode
    //   skipInput = false;             // No other inputs allowed in CPM

    if ((aByte == 0) && !pauseFlag) { // When transmit queue is empty
      codePractice(CPM);         // Manufacture five more characters
    }

    // Button checks specific to Code Practice Mode
    // Check for request to change CPM speed
    if (!Button(UP)) {           // Increase WPM value
      while (!Button(UP)) {}     // Spin until released
      wordsPerminute += 1;
      // ??? write WPM to eeprom
    }

    if (!Button(DOWN)) {         // Decrease WPM value
      while (!Button(DOWN)) {}   // Spin until released
      wordsPerminute -= 1;
      // ??? write WPM to eeprom
    }

    if (wordsPerminute > MAXWPM) wordsPerminute = MAXWPM; // range check
    if (wordsPerminute < MINWPM) wordsPerminute = MINWPM; // range check
    dotLength = 1200 / wordsPerminute; // Set the element length

    // Check for pause button in CPM, this toggles display blinking
    if (!Button(ENTER)) {
      while (!Button(ENTER)) {}  // Spin until released
      pauseFlag = !pauseFlag;    // Toggle pause request flag
      lastPause = pauseFlag;     // So pause state can be restored after menu

      if (pauseFlag) {           // Pause no longer in effect
        nosineTone();            // Turn off any sidetone
      } else {                   // Pause is in effect
        analogWrite(BACKLITE, lcdBackLite); // Restore original brightness
        currentBack = lcdBackLite; // Remember the setting
      }
    }                            // End pause button check

    // Check buttons 1-4 for Farnsworth (extra character) spacing
    // FarnsWorthiness is applied as an inter character space multiplier
    for (int i = firstButton; i <= firstButton + 3; i++) {
      if (!Button(i)) {
        farnsWorthiness = i - firstButton; // Caught a button down
      }
    }
  }                              // End CPM tests

  // Blink the backlight if paused and not displaying status
  if (pauseFlag && (nextUpdate == 0)) { // Blink the backlight?

    if (!((Mtime & 0x0700) != 0)) { // Will be off 1/8 of time
      analogWrite(BACKLITE, MINLCD);
      currentBack = MINLCD;      // Remember the setting
    } else {
      analogWrite(BACKLITE, lcdBackLite);
      currentBack = lcdBackLite; // Remember the setting
    }
  }

  // Check if memories need to be played or recorded
  // The loop depends on seven analog pins being sequentially assigned
  // Scan only if no button is already pending
  if (pressed == 0) {

    for (int i = firstButton; i <= lastButton; i++) { // Scan the memory buttons

      if (!Button(i)) {
        pressed = i;             // Caught a button down

        if (!Button(FUNCTION)) { // Is the func button pressed?
          recordMessage (pressed); // Call the message write routine
          pressed = 0;           // Deactivate that button
        }
        break;                   // Abort the scan
      }
    }
  }

  if (skipInput) {               // Skip external inputs in CPM or status display

    if (pressed) {               // Any button pressed?
      if (Button(pressed)) {     // Goes true when button released
        // insert into send queue the pin number of pressed button
        storeInBuffer((unsigned char)pressed);
        pressed = 0;             // Reset the button pressed
      }
    }

    // poll the two possible keyboards
    gotCharkey = false;
    if (Serial.available()) {    // Process a serial char if present
      inByte = Serial.read();    // Get one byte
      gotCharkey = true;         // Remember we have a character pending
    }

    if (keyboard.available()) {  // Process a PS2 char if present
      inByte = keyboard.read();  // Get one byte
      gotCharkey = true;         // Remember we have a character pending
    }

    // If keyboard was read, do something with it
    if (gotCharkey) {
      processByte(inByte);       // Check and Queue this ASCII character
    }
  }                              // End of status or code practice exclusion

  // Do either Straight Key Mode or Iambic Mode
  if (skMode) {
    //   if (!pauseFlag) doIambicKey(); // Stop morse engine if paused
    doIambicKey(); // Stop morse engine if paused
  }
  else {
    doStraightKey();
  }
}                                // End of loop()


/********************************************************
   Iambic keyer mode
   Original code is from KC4IFB's work published in QEX
   is slightly modified to also send morse from a keyboard
   or memory, change sending speed, or sidetone frequency.
   Global keyTherig false blocks rig keying while programming
   memories or while in code practice mode.
   GLobal keyTherigQ blocks keying until the Queue is empty.
 ********************************************************/
void doIambicKey() {

  static byte mByte = 0;         // Holds an encoded morse byte being sent
  static int aPos = 0;           // Pointer to ASCII byte in message memory
  static unsigned long idleEntry; // Records time that idle state was entered
  static unsigned long charSpacetime = Mtime; // Time for character space
  static unsigned long wordSpacetime = Mtime; // Time for word space
  static boolean spaceNeeded = false;// Records if an ascii space has been issued
  static byte bitMask = 0x01;    // Used to assemble MtoA table index
  static int tabIndex = 0;       // Assembled index into MtoA table

  // Poll the paddles *only* if no queued ASCII byte is pending
  // aByte is set to zero if the Queues are empty
  if (aByte == 0) {
    dotVal = digitalRead(DOTIN);
    dashVal = digitalRead(DASHIN);
  } else {
    dotVal = dashVal = HIGH;     // Force paddle open
  }
  // Check func button to see if speed mode requested
  // Func held down means set speed mode
  // But NOT if in code practice mode or playing from transmit queue
  if (!Button(FUNCTION) && (CPM == 0) && (aByte == 0)) {
    inSpeed = true;              // Speed mode flag true = in set speed mode
    digitalWrite(KEYOUT, LOW);   // Unkey the transmitter (and LED)
  }
  else inSpeed = false;          // NOT in speed set mode

  // Preserve speed in eeprom but write only when speed mode is exited
  // lastSpeed is set only if speed was actually changed
  if (!inSpeed && lastSpeed) {   // NOT in speed mode now
    wpmToeeprom();               // Writes Words Per Minute to eeprom
    lastSpeed = false;
  }

  switch (currElt) {             // Four cases based on current state

    case DASH:

      if ((dotVal == LOW) && (nextElt == IDLE)) { // Going to iambic mode
        nextElt = DOT;
      }

      if (Mtime >= currEltendtime) { // at end of current dash
        lastElt = DASH;
        currElt = DELAY;         // a delay will follow the dash
        currEltendtime = Mtime + dotLength;

        if (inSpeed) {
          changeSpeed( -1 );
          lastSpeed = true;
        }
      }

      // close keyer output and start sidetone while dash is sent
      if (!toneOn) {             // flag ensures tone is only turned on once
        //       if (!inTone && !inSpeed && keyTherig && keyTherigQ) {
        if (!inSpeed && keyTherig && keyTherigQ) {
          digitalWrite(KEYOUT, HIGH);
        }
        sineTone(sideTone);      // used to be the Arduino tone()
        toneOn = true;           // remembers side tone is on and rig is keyed
        //Serial.println("-");
        // add a dash bit into tabIndex for morse decode
        if (aByte == 0) {        // but only if sending from paddles
          // a dash adds in a zero (just shift mask left)
          if ((bitMask & 0x40) != 0x40) {  // no shift if at max length (7 bits)
            bitMask = bitMask << 1; // move mask to next higher bit position
          }
        }
      }
      break;                     // end of DASH case

    case DOT:

      // note if bitmask is not 0x01 then a character is in progress from paddles
      if ((dashVal == LOW) && (nextElt = IDLE)) {  // going to iambic mode
        nextElt = DASH;
      }

      if (Mtime >= currEltendtime) { // at end of current dot
        lastElt = DOT;           // a delay will follow the dot
        currElt = DELAY;
        currEltendtime = Mtime + dotLength;

        if (inSpeed) {
          changeSpeed( +1 );
          lastSpeed = true;
        }
      }

      // close keyer output and start sidetone while dot is sent
      if (!toneOn) {             // flag ensures tone is only turned on once
        //       if (!inTone && !inSpeed && keyTherig && keyTherigQ) {
        if (!inSpeed && keyTherig && keyTherigQ) {

          digitalWrite(KEYOUT, HIGH);
        }
        sineTone(sideTone);      // used to be the Arduino tone()
        toneOn = true;           // remembers that side tone is on and rig is keyed
        //Serial.println(".");
        // add a dot (a one) into tabIndex for morse decode
        if (aByte == 0) {        // but only if sending from paddles
          tabIndex = tabIndex | bitMask;   // adds in a one
          if ((bitMask & 0x40) != 0x40) {  // no shift if at max length 7 bits
            bitMask = bitMask << 1; // move mask to next higher bit position
          }
        }
      }
      break;                     // end of DOT case

    case IDLE:                   // not sending, nor delay after a dot or dash

      // Are either (or both) of the paddles closed?
      if ((dotVal == LOW) && (dashVal == HIGH)) { // only dot closed
        lastElt = IDLE;
        currElt = DOT;           // set DOT mode
        currEltendtime = Mtime + dotLength;
        return;
      }

      else if ((dotVal == HIGH) && (dashVal == LOW)) { // only dash closed
        lastElt = IDLE;
        currElt = DASH;          // set DASH mode
        currEltendtime = Mtime + (3 * dotLength);
        return;
      }

      // *neither* paddle is closed
      // the morse character *may* be complete
      if (enteringIdle) {        // first time on this entry?
        idleEntry = Mtime;       // mark entry time for space triggers
        enteringIdle = false;    // only mark the time once
        // time at which a character space will be triggered
        charSpacetime = idleEntry + dotLength * CSPACEWAIT;
        // time at which a word space will be triggered
        wordSpacetime = idleEntry + dotLength * WSPACEWAIT;
      }

      // don't bother printing assembled character or space if
      // character was from queue or from memory message
      if (aByte == 0) {          // Char is in progress & Q is empty

        // delay before finishing an ascii decode from paddles
        // if paddles were active, bitMask will have been left shifted
        if ((bitMask != 0x01) && (Mtime > charSpacetime)) { // from paddles

          // there is a morse character waiting and idle time has expired
          // finish assembling the morse decode index, lookup ASCII and print
          tabIndex = tabIndex | bitMask;  // or in the final stop bit
          asciiChar = MtoA[tabIndex]; // get ASCII translation
          echoChar(inByte = (int)asciiChar); // print ASCII equiv of morse char

          // memory writes from paddles are lower case, else chars run together
          if (inByte >= 0x41) inByte += 0x20;
          gotCharpad = true;     // flag for possible eeprom write
          bitMask = 0x01;        // re-initialize the index mask
          tabIndex = 0x00;       // and re-initialize the index
          spaceNeeded = true;    // a space character MAY be needed
        }

        // if a sufficiently long IDLE period has elapsed and the paddles
        // haven't started another character, print and store a word space
        if (spaceNeeded && (Mtime > wordSpacetime)) {
          echoChar(0x20);        // send a space to serial terminal
          inByte = 0x20;         // space for eeprom write subroutine
          gotCharpad = true;     // flag for eeprom write subroutine
          spaceNeeded = false;   // record that one space was issued
        }
      }

      // Done with previous character, check for pending action.
      // The following sorts out four possible states:
      // - We are in the middle of a button message, get next ASCII from EEPROM
      // - There is no memory message in progress and Q is empty
      // - Q not empty, and Q contains ASCII (>= 0x20)
      // - Q not empty, and Q contains a memory message pin number (< 0x20)
      if (aPos) {                // aPos active, memory message in progress
        // get next ASCII byte from message memory until NULL
        aByte = EEPROM.read( ++aPos );

        if (aByte) {             // if not NULL, get morse byte
          mByte = initByte(aByte); // check and lookup morse
        }
        else {                   // got null, end of message
          aByte = 0x20;          // Force space at end of memory message
          aPos = 0;              // position of zero indicates not reading msg
          keyTherigQ = true;     // allow rig keying after speed or tone set
        }
        break;                   // skip queue check until msg is completed
      }

      if (head != tail) {        // Queue not empty, get new char started
        aByte = readFromBuffer(); // get next byte from circular Queue

        if (aByte >= 0x20) {     // >= space, byte must be ASCII
          mByte = initByte(aByte); // check and lookup morse
        }
        else {                   // Q contained a memory button indicator
          aPos = ((aByte - firstButton) * MSGSIZE) + 8; // point to start of msg
          aByte = EEPROM.read(aPos); // get the first ASCII byte of the message
          mByte = initByte(aByte); // check and lookup morse
        }                        // end message button initialize
      }                          // end head != tail
      else {
        aByte = 0;               // Signal that Queue was empty
        keyTherigQ = true;       // allow rig keying after speed or tone set
      }
      break;                     // end the IDLE case

    case DELAY:                  // Element length delay after dot or dash

      if (Mtime >= currEltendtime) { // delay time expired?
        // delay has expired, set a new current element and time
        currElt = nextElt;

        // check if we are playing from the queue
        if (mByte != 0) {        // not empty, pull next elt from morse byte

          if (mByte != 0x40) {   // not finished with this morse byte
            // mask and read bit six from morse byte
            if (mByte & 0x40) {  // need a dit next
              currElt = DOT;
            }
            else {               // need a dah next
              currElt = DASH;
            }
            mByte = (mByte << 1) & 0x7f;// lshift, truncate to right seven bits
          }                      // finished set morse bit

          else {                 // last element bit has been shifted out
            echoChar(aByte);     // print the ASCII from queue or msg memory
            // make an intercharacter space by extending DELAY two element times
            // but only if lower case, (so Prosigns can be sent run together)
            if ((aByte < 0x41) || (aByte > 0x5A)) {
              // Add additional time if in Code Practice Mode
              currEltendtime = Mtime + dotLength * (2 + farnsWorthiness * 2);
              currElt = DELAY;
            }
            mByte = 0;           // marks that morse byte is done
            break;
          }
        }

        // set up the timeouts
        if (currElt == DOT) {
          currEltendtime = Mtime + dotLength;
        }
        else if (currElt == DASH) {
          currEltendtime = Mtime + (3 * dotLength);
        }
        lastElt = DELAY;
        nextElt = IDLE;          // default next element is IDLE
      }

      // during the delay, if either paddle is pressed, save it to play after the delay
      if ((lastElt == DOT) && (dashVal == LOW) && (nextElt == IDLE)) {
        nextElt = DASH;          // override default IDLE
      }
      else if ((lastElt == DASH) && (dotVal == LOW) && (nextElt == IDLE)) {
        nextElt = DOT;           // override default IDLE
      }

      // note there is always a DELAY before IDLE
      if (toneOn) {              // do these things only once on entry
        // key output is open during the delay
        digitalWrite(KEYOUT, LOW);
        nosineTone();            // used to be the Arduino noTone()
        toneOn = false;
        enteringIdle = true;     // NOT in idle mode yet
      }
      break;                     // end of case DELAY
  }                              // end of switch currElt
}                                // End of Iambic Keyer mode


/********************************************************
   Converts an ASCII character originating from keyboard
   or one of the message memories to a morse encoded byte.
   Returns morse encoded byte from ASCII to Morse table (AtoM.h).
   Sets global current element and shifts for next bit read.
   Subsequent element bits are shifted in and read in DELAY.
 ********************************************************/
byte initByte(byte aByte) {
  byte mByte;                    // holds an encoded morse byte being sent

  if (aByte >= 0x61) {           // ASCII byte is lower case
    aByte -= 0x20;               // shift lc to uc for table lookup
  }
  // ASCII 0x20 (space) is item zero in the table
  mByte = AtoM[aByte - 0x20];    // scale down

  // initialize current Element with bit six from new morse byte
  // further bit extracts and shifts are in DELAY
  if (mByte != 0x40) {           // is NOT a space
    if (mByte & 0x40) {          // need a dit next
      currElt = DOT;
      currEltendtime = Mtime + dotLength;
    }
    else {                       // need a dah next
      currElt = DASH;
      currEltendtime = Mtime + (3 * dotLength);
    }
    lastElt = IDLE;
    mByte = (mByte << 1) & 0x7f; // lshift, truncate to right seven bits
  }
  else {                         // special for space = morse 0x40
    currElt = DELAY;
    currEltendtime = Mtime + (3 * dotLength);
  }
  enteringIdle = true;           // NOT in idle mode
  return mByte;
}


/********************************************************
   validate and scale an ASCII keyboard character for sending
 ********************************************************/
void processByte(int inByte) {

  // add line break to console screen if cr/lf received from keyboard
  if ((inByte == 0x0a) || (inByte == 0x0d)) {
    charsOut = MAXLINE;          // trigger a newline
  }

  if (inByte == 0x005c) {        // backslash received
    commandMode = true;          // activate keyboard command mode
    echoChar((char)inByte);      // echo the backslash
  }

  // ignore the incoming byte if out of range
  // note backslash *is* out of range
  if (((inByte >= 0x20) && (inByte <= 0x5a)) || ((inByte >= 0x61) && (inByte <= 0x7a))) {
    if (commandMode) processCommand (inByte); // detour to command function
    else storeInBuffer(inByte); // add this character to the send queue
  }
}


/********************************************************
   Enter keyboard command mode and process commands
   Keyboard commands are a backslash followed by a single letter
   only one command at a time is processed
 ********************************************************/
void processCommand(int inChar) {

  echoChar(inChar);

  switch ( inChar) {

    case 'w':                    // force an EEPROM write
      wpmToeeprom();
      sidetoneToeeprom();
      inTone = false;
      lcd.clear();
      lastTone = false;
      break;

    case '+':                    // ASCII plus sign (speed up)
      changeSpeed( +1 );
      storeInBuffer ('S');       // send something so user can judge speed
      keyTherigQ = false;        // but suppress rig keying until Q is empty
      break;

    case '=':                    // Same as plus sign but no shift
      changeSpeed( +1 );
      storeInBuffer ('S');       // send something so user can judge speed
      keyTherigQ = false;        // but suppress rig keying until Q is empty
      break;

    case '-':                    // ASCII minus sign (speed down)
      changeSpeed( -1 );
      storeInBuffer ('S');       // send something so user can judge speed
      keyTherigQ = false;        // but suppress rig keying until Q is empty
      break;

    case 'u':                    // "u" key (tone UP)
      changeFreq( +1 );
      storeInBuffer ('T');       // send something so user can judge frequency
      keyTherigQ = false;        // but suppress rig keying until Q is empty
      break;

    case 'd':                    // "d" key (tone DOWN)
      changeFreq( -1 );
      storeInBuffer ('T');       // send something so user can judge frequency
      keyTherigQ = false;        // but suppress rig keying until Q is empty
      break;

    case '1':                    // Send canned message 1
      storeInBuffer(firstButton);
      break;

    case '2':                    // send canned message 2
      storeInBuffer(firstButton + 1);
      break;

    case '3':                    // send canned message 3
      storeInBuffer(firstButton + 2);
      break;

    case '4':                    // send canned message 4
      storeInBuffer(firstButton + 3);
      break;

    case '5':                    // send canned message 5
      storeInBuffer(firstButton + 4);
      break;

    case '6':                    // send canned message 6
      storeInBuffer(firstButton + 5);
      break;

    case '7':                    // send canned message 7
      storeInBuffer(firstButton + 6);
      break;

    default:                     // command char not recognized
      break;                     // Just ignore it

  }                              // end switch

  commandMode = false;           // always cancel command mode
}


/***************************************************************
   Place a byte of data into the ring buffer
   will not overrun the buffer
 **************************************************************/
void storeInBuffer(unsigned char data)
{
  unsigned int next = (unsigned int)(head + 1) % MAXQ;
  if (next != tail)
  {
    sendQ[head] = data;
    head = next;
  }
}


/***************************************************************
   Retrieve a byte of data from the ring buffer
   returns -1 if the buffer was empty
 **************************************************************/
char readFromBuffer()
{
  // if the head isn't ahead of the tail, we don't have any characters
  if (head == tail) {
    return -1;                   // quit with an error
  }
  else {
    char data = sendQ[tail];
    tail = (unsigned int)(tail + 1) % MAXQ;
    return data;
  }
}


/********************************************************
   Straight Key mode
   Blindly follow the DOTIN lead (jack tip)
 ********************************************************/
void doStraightKey(void) {
  static unsigned long debounceTime;

  // Don't read the key again until the debounce period expires
  if (millis() < debounceTime) return;

  if (!digitalRead(DOTIN)) {     // Key is closed

    if (!toneOn) {               // flag ensures tone is only turned on once

      // turn on the output and the tone
      digitalWrite(KEYOUT, HIGH); // close the keyer output
      sineTone(sideTone);
      toneOn = true;             // remember that side tone is on and rig is keyed
      debounceTime = Mtime + 5;
    }
  }
  else {                         // Key is open
    if (toneOn) {                // flag ensures tone is only turned off once

      // turn off tone and unkey output
      digitalWrite(KEYOUT, LOW); // unkey the rig
      nosineTone();
      toneOn = false;            // Remember that tone is off and rig is unkeyed
      debounceTime = Mtime + 5;
    }
  }
}                                //  End of Straight Key mode


/********************************************************
   change the morse sidetone frequency
 ********************************************************/
void changeFreq(int freqSign) {

  sideTone += (freqSign * sideTone) / 20;      // increment or decrement 5%

  if (sideTone > MAXTONE) sideTone = MAXTONE;  // range check max
  if (sideTone < MINTONE) sideTone = MINTONE;  // range check min

  // Display the changed side tone frequency
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Tone Freq ");
  lcd.print(sideTone);
  lcd.setCursor(0, 0);
}


/********************************************************
   change the morse sending speed
 ********************************************************/
void changeSpeed(int speedSign) {

  wordsPerminute += speedSign;   // increment or decrement speed

  if (wordsPerminute > MAXWPM) wordsPerminute = MAXWPM; // range check max
  if (wordsPerminute < MINWPM) wordsPerminute = MINWPM; // range check min

  // Display the changed Words Per Minute
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Speed is ");
  lcd.print(wordsPerminute);
  dotLength = 1200 / wordsPerminute;
  lcd.setCursor(0, 0);
}


/********************************************************
   update the stored sidetone frequency information in EEPROM
 ********************************************************/
void sidetoneToeeprom (void) {
  lcd.clear();
  Serial.print("\n");
  Serial.print("Write Sidetone ");
  lcd.setCursor(0, 0);
  lcd.print("Write Sidetone  ");
  Serial.print(sideTone);
  Serial.println(" to EEProm");
  lcd.setCursor(0, 1);
  lcd.print(sideTone);
  lcd.print(" to EEProm  ");
  lcdChars = 0;                  // Reset LCD characters emitted

  // sideTone is an INT, its range requires two bytes
  EEPROM.write(0, (byte)(sideTone & 0xFF)); // record speed in EEPROM lo byte
  EEPROM.write(1, (byte)(sideTone >> 8));  // record speed in EEPROM hi byte
  delay(750);                    // Pause to read message
  lcd.clear();
}


/********************************************************
   update the stored speed information in EEPROM
 ********************************************************/
void wpmToeeprom (void) {
  lcd.clear();
  Serial.print("\n");
  Serial.print("Write Speed ");
  lcd.setCursor(0, 0);
  lcd.print("Write Speed   ");

  Serial.print(wordsPerminute);
  lcd.setCursor(0, 1);
  lcd.print(wordsPerminute);

  Serial.println(" WPM to EEProm");
  lcd.print(" WPM to EEProm");
  lcdChars = 0;                  // Reset LCD characters emitted

  // WordsPerminute is an int but its range only needs one byte
  EEPROM.write(2, (byte)(wordsPerminute)); // record speed in EEPROM lo byte
  delay(750);                    // Pause to read message
  lcd.clear();
}


/********************************************************
   Echo character to serial port and new line if > 80 characters sent
   Also prints to Liquid Crystal
 ********************************************************/
void echoChar(int inByte) {

  if (++charsOut > MAXLINE) {    // trigger newline
    Serial.println();
    charsOut = 0;
  }
  Serial.print((char)inByte);

  // !#$@ LCD has no backslash character so suppress kbd commands
  if (!commandMode) {              // No LCD if its a command
    // Hack to get decent scrolling on the LCD
    if (!(lcdChars++ & 0x000f)) {  // Modulo 16 columns
      if (lcdChars & 0x0010) {     // Is odd 16 or even 16?
        lcd.setCursor(0, 1);       // Print odd 16 on second line
      } else {
        lcd.setCursor(0, 0);       // Print even 16 on first line
      }
    }
    lcd.print((char)inByte);
    Keyboard.print((char)inByte);
  }
}


/********************************************************
   Get a message from the keyboard OR PADDLES and record it
   to EEPROM, is tripped when Func and one of the memory
   buttons are both down then releases. Func alone to finish.
 ********************************************************/
void recordMessage (int whichButton) {

  // spin wait for memory button to be unpressed
  while (!Button(whichButton)) {
  }

  // spin wait for function button to be unpressed
  while (!Button(FUNCTION)) {  }

  Serial.println();
  Serial.print("Record Memory ");
  Serial.println(whichButton - firstButton + 1);
  lcd.clear(); lcd.setCursor(0, 1);
  lcd.print("Record Memory ");
  lcd.print(whichButton - firstButton + 1);
  lcd.setCursor(0, 0);           // Move to first line of LCD
  lcdChars = 0;                  // Move to first line of LCD

  charsOut = MAXLINE;            // Will trigger a new line on serial console

  beeps(LOWTONE);                // Announce entering program mode
  keyTherig = false;             // Do NOT key the radio
  // calculate where this button's message starts in eeprom
  int startAddress = (whichButton - firstButton) * MSGSIZE + 8;
  int memBase = startAddress;    // MemBase will increment as chars are added

  // The following statement hijacks the main loop while recording memory messages
  while (Button(FUNCTION)) {     // Get chars until func is pressed again

    Mtime = millis();            // Get the current time
    waveShape();                 // Manage the sidetone generation

    gotCharkey = false;          // Will be set if character from keyboard
    gotCharpad = false;          // Will be set if character from paddles

    // poll the serial keyboard
    if (Serial.available()) {    // Process a serial char if present
      inByte = Serial.read();
      gotCharkey = true;
    }

    // poll the PS2 keyboard
    if (keyboard.available()) {  // Process a PS2 char if present
      inByte = keyboard.read();
      gotCharkey = true;
    }

    // poll the paddles, transmitter keying is supressed
    doIambicKey();

    if (gotCharkey || gotCharpad) { // There is something to do

      if (gotCharkey) processByte(inByte); // Beep out only if keyboard

      if ((inByte != 0x0a) && (inByte != 0x0d)) { // Ignore CR and LF
        EEPROM.write(memBase++, (byte)inByte);    // Write and increment
        // max memory string is MSGSIZE char plus NULL, stall if greater
        if (memBase > (startAddress + MSGSIZE - 1)) memBase = startAddress + MSGSIZE - 1;
      }
    }
  }                              // End while()

  // hHre when Func is pressed again, spin wait for func to be unpressed
  while (!Button(FUNCTION)) {
  }

  if ((memBase - startAddress) > 0) {
    EEPROM.write(memBase, 0x00);   // Complete the string in EEPROM
  }

  Serial.println();
  Serial.print("Memory ");
  Serial.print(whichButton - firstButton + 1);
  Serial.print(", ");
  Serial.print(memBase - startAddress);
  Serial.print(" ");

  lcd.clear(); lcd.setCursor(0, 0);
  lcd.print("Memory ");
  lcd.print(whichButton - firstButton + 1);
  lcd.print(", ");
  lcd.print(memBase - startAddress);

  Serial.println("Chars Written");
  lcd.setCursor(0, 1);
  lcd.println("Chars Written   ");

  lcd.setCursor(0, 0);           // Move to first line of LCD
  lcdChars = 0;

  charsOut = MAXLINE;            // Will trigger a new line on serial terminal
  beeps(LOWTONE);                // Announce completion program mode
  keyTherig = true;              // Reenable keying the radio
}


/********************************************************
   Message button 1, 2, or 3 was down when Arduino was reset
   enter code practice mode. The only exit is to reset again.
   Practice character groups are stuffed into the send queue.
 ********************************************************/
void codePractice (int inButton) {
  int ranChar;                   // a random char picked with rannum
  int outCount = 0;              // count chars placed in send queue
  static int groups = -1;        // count 5 char groups completed
  keyTherig = false;             // do NOT key the radio

  inButton -= firstButton;       // reference button to zero

  while (outCount++ < 5) {       // make five random chars

    switch (inButton) {

      case 0:                    // button 1 ltrs only
        ranChar = random (26);   // ASCII ltrs range
        storeInBuffer(ranChar + 0x61);
        break;

      case 1:                    // button 2 ltrs + figs
        ranChar = random(36);    // universe of ltrs+figs

        if (ranChar < 26) {      // letter range
          storeInBuffer(ranChar + 0x61);
        }
        else {                   // number range
          storeInBuffer(ranChar - 26 + 0x30);
        }
        break;

      case 2:                    // button 3 ltrs + figs + punct Oh My!
        // Many ASCII punctuation chars have no morse equivalent so we
        // ignore the initial choice and use the Pinata Algorithm (flail
        // around in the dark until you hit something good).
        while ((unsigned int)AtoM[ranChar = random(59)] == 0x40) {
        }
        if (ranChar > 0x20) ranChar += 32; // convert upper case to lower
        storeInBuffer(ranChar + 0x20);
        break;
    }
  }

  // finally stuff some (defined in Canned.h) delay time in the sendQueue
  for (int i = 0; i < PRACPAUSE; i++) { // see Canned.h
    storeInBuffer(' ');          // store trailing spaces
  }

  if ((++groups % 4) == 0) {
    Serial.print(F("\ngroup "));
    pz(groups + 1, 4);           // seperate into lines of four groups
    Serial.print("   ");
    lcdChars = charsOut = outCount = 0;
  }
}


/********************************************************
   just to get leading zeros on a four digit integer - Serial
 ********************************************************/
void pz(int k, int digits) {
  switch (digits) {
    case 4:
      if (k < 1000 ) Serial.print("0");
    case 3:
      if (k < 100 ) Serial.print("0");
    case 2:
      if (k < 10 ) Serial.print("0");
    default:
      Serial.print(k);
  }
}


/********************************************************
   Just to get leading zeros on up to four digit integer - LCD
     K is variable to print
     digits is # places to use
 ********************************************************/
void lcdPz(int k, int digits) {
  switch (digits) {
    case 4:
      if (k < 1000 ) lcd.print("0");
    case 3:
      if (k < 100  ) lcd.print("0");
    case 2:
      if (k < 10   ) lcd.print("0");
    default:
      lcd.print(k);
  }
}


/********************************************************
   Announcement tones, 3 quick beeps
 ********************************************************/
void beeps(int freq) {

  for (int i = 0; i <= 2; i++) {
    // generate tone using timers, not tone()
    unsigned long onTime = millis() + 60;
    unsigned long offTime = onTime + 50;

    sineTone(freq);
    while ((Mtime = millis()) < onTime) {
      waveShape();               // Spin & waveshape
    }

    nosineTone();
    while ((Mtime = millis()) < offTime) {
      waveShape();               // Spin & waveshape
    }

    freq = (freq * 6) / 5;       // Raise by a minor third
  }
}


/********************************************************
   Set up to turn ON the synthesized sine wave tone
   This function replaces arduino tone()
   The tone is actually turned on and off in waveShape()
 ********************************************************/
void sineTone(int freq) {

  setupPeriod(freq);
  Mtime = millis();
  rampUp = true;                 // enable waveshaping for leading edge
  T0 = Mtime;                    // turn on tone at half amplitude
  T1 = Mtime + 1;                // 1 ms later to 3/4 amplitude
  T2 = Mtime + 2;                // 1 ms later to full amplitude
}


/********************************************************
   Set up to turn OFF the synthesized sine wave tone
   This function replaces arduino noTone()
   The tone is actually turned on and off in waveShape()
 ********************************************************/
void nosineTone() {

  Mtime = millis();
  rampDown = true;               // enable waveshaping for trailing edge
  T0 = Mtime;                    // initiate ramp down
  T1 = Mtime + 1;                // 1 ms later down further
  T2 = Mtime + 2;                // 1 ms later turn it off
}


/********************************************************
   Output tone is managed in this function with envelope
   shaping for leading and trailing edges. Amplitude is
   changed in three steps by switching between sine lookup tables.
 ** This function must be called repeatedly in an event loop to work. **
   Note rampDown leaves half amplitude table set during idle period.
 ********************************************************/
void waveShape() {

  // Shaping for tone leading edge envelope
  if (rampUp) {                  // Start of element tone generation

    if (Mtime > T2) {            // Final switch to full amplitude
      sineTable = sineTable1_0;  // change table with interrupt disabled

      rampUp = false;            // done leading edge waveshaping
    }
    else if (Mtime > T1) {       // 3/4 amplitude later at time 1
      sineTable = sineTable3_4;  // change table with interrupt disabled
      T1 = 0xffffffff;           // so it only does this once
    }
    else if (Mtime > T0) {       // Turn on the wave at time zero
      waveCounter = 0;           // Preset sine synth phase to zero
      waveTimer.begin (MakeSineISR, tablePeriod); // Start synth
      T0 = 0xffffffff;           // so it only does this once
    }
  }

  // Shaping for tone trailing edge envelope
  if (rampDown) {                // End of element tone period

    if (Mtime > T2) {            // Final time expired, turn off wave

      waveTimer.end ();
      analogWrite(A12, 128);     // fix idle output to half scale
      rampDown = false;          // done trailing edge waveshaping
    }
    else if (Mtime > T1) {       // later switch to half amplitude
      sineTable = sineTable1_2;  // change table with interrupt disabled
      T1 = 0xffffffff;           // so it only does this once
    }
    else if (Mtime > T0) {       // At time zero switch to 3/4 table
      sineTable = sineTable3_4;  // change table with interrupt disabled
      T0 = 0xffffffff;           // so it only does this once
    }
  }
}


/********************************************************
   Calculate Sine period and table spacing in microseconds
 ********************************************************/
void setupPeriod(int ddsFrequency) {
  unsigned int sinePeriod;       // sine wave period microseconds

  sinePeriod = 1000000 / ddsFrequency;  // wave period in Usec
  tablePeriod = sinePeriod / (bitMask64 + 1); // table step period
}


/********************************************************
   Interrupt service routine (ISR) used to generate
   sine wave from the DAC. See docs on PJRC's intervalTimer
   Quarter symmetry code optomized with methods shown at
   http://www.fpga4fun.com/DDS2.html
 ********************************************************/
void MakeSineISR() {
  unsigned char DACValue;        // Sent to the DAC by ISR
  // These are fixed and used in the ISR
  const unsigned int halfMaskbit = (bitMask64 + 1) >> 1;// To find which half of sine
  const unsigned int qtrMaskbit = (bitMask64 + 1) >> 2; // To find which quarter
  const unsigned int qtrMask = bitMask64 >> 2;   // # of entries in 1/4 wave

  // Look up current amplitude value for the sine wave being constructed.
  // First, quarter wave symmetry from FPGA example
  // sine_2sym = addr10_delay2 ? {1'b0,-sine_1sym} : {1'b1,sine_1sym};
  if (waveCounter & qtrMaskbit) { // In second or fourth quarter
    DACValue = *(sineTable + (~waveCounter & qtrMask)); // Read table backwards
  } else {                       // In first or third quarter
    DACValue = *(sineTable + (waveCounter & qtrMask)); // Read table forwards
  }

  // Invert the second half of sine wave from FPGA example
  // .rdaddress(addr[9] ? ~addr[8:0] : addr[8:0])
  if (waveCounter & halfMaskbit) {
    DACValue = ~DACValue;
  }
  analogWrite(TONE, DACValue);   // Write to the DAC
  waveCounter = (waveCounter + 1) & bitMask64; // Set up for next interrupt
}


/********************************************************
   Print time data to LCD
    arg: Unix clock time
    arg: Dont print seconds if 0, else print seconds
 ********************************************************/
void digTimeDispLCD(time_t clockT, int secDigits) {

  lcdPz(hour(clockT), 2);
  lcd.print(":");
  lcdPz(minute(clockT), 2);

  if (secDigits != 0) {
    lcd.print(":");
    lcdPz(second(clockT), 2);
  }
}


/********************************************************
   Print time data to Serial
    arg: Unix clock time
    arg: Dont print seconds if 0, else print seconds
 ********************************************************/
void digTimeDispSerial(time_t clockT, int secDigits) {

  pz(hour(clockT), 2);
  Serial.print(":");
  pz(minute(clockT), 2);

  if (secDigits != 0) {
    Serial.print(":");
    pz(second(clockT), 2);
  }
}


/********************************************************
   Print Year data to LCD
    arg: unix clock time
    arg: numb of year digits to print or not
         0 supresses year printing
 ********************************************************/
void digDateDispLCD(time_t clockT, int yrDigits) {

  lcdPz(month(clockT), 2);
  lcd.print("/");
  lcdPz(day(clockT), 2);

  if (yrDigits != 0) {
    lcd.print("/");

    switch (yrDigits) {
      case 1:
        lcd.print(year(clockT) % 1000);
        break;
      case 2:
        lcd.print(year(clockT) % 100);
        break;
      case 3:
        lcd.print(year(clockT) % 10);
        break;
      default:
        lcd.print(year(clockT));
        break;
    }
  }
}


/********************************************************
   Print Year data to serial
    arg: unix clock time
    arg: numb of year digits to print or not
         0 supresses year printing
 ********************************************************/
void digDateDispSerial(time_t clockT, int yrDigits) {

  pz(month(clockT), 2);
  Serial.print("/");
  pz(day(clockT), 2);

  if (yrDigits != 0) {
    Serial.print("/");

    switch (yrDigits) {
      case 1:
        Serial.print(year(clockT) % 1000);
        break;
      case 2:
        Serial.print(year(clockT) % 100);
        break;
      case 3:
        Serial.print(year(clockT) % 10);
        break;
      default:
        Serial.print(year(clockT));
        break;
    }
  }
}

/********************************************************
   This from TimeTeensy3 example somehow sets the system clock
 ********************************************************/
time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}


/********************************************************
  Code to accept time sync messages from the serial port
 ********************************************************/
unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if (Serial.find(TIME_HEADER)) {
    pctime = Serial.parseInt();
    return pctime;
    if ( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
      pctime = 0L;               // return 0 to indicate that the time is not valid
    }
  }
  return pctime;
}


/********************************************************
   Calc and print battery status
   Prints to serial in a spreadsheetable format
   To log a discharge curve, need a USB cable
     with the red wire disconnected.
 ********************************************************/
void chargeStatus(void) {
  int lowBatt;                   // Low battery alarm
  int usbPwr;                    // USB power supplied
  int chgStat;                   // Charging status

  // calibrate pot set for incoming voltage times eight
  // which will display actual voltage
  // e.g. 4.00 volts = 3200 from 12 bit ADC
  // Just put voltmeter on battery and adjust pot
  // so Voltage reads the same.
  Serial.print(batVolts / 8); Serial.print(" Voltage ");
  Serial.print(batVoltsSmoothed / 8); Serial.print(" Voltage_Smoothed ");
  lowBatt = digitalRead(lowBattPin);
  Serial.print(lowBatt); Serial.print(" Low_Batt ");
  usbPwr = digitalRead(usbPwrPin);
  Serial.print(usbPwr); Serial.print(" USB_Supply ");
  chgStat = digitalRead(chgStatPin);
  Serial.print(chgStat); Serial.println(" Chg_Status");

  lcd.setCursor(0, 1);
  // Analog battery voltage: calibrate with a DVM across battery
  // Adjust calibrate pot on charger until display agrees with DVM
  lcd.print(batVoltsSmoothed / 8); // for 12 bit ADC

  // Serious trouble
  if (lowBatt == 0) {            // Low battery pin is LOW
    lcd.print(" DEAD BATT!!!");
    return;                      // Bail out
  } else
    // Trouble
    if (lowBattFlag) {           // Software low batt is on
      lcd.print(" LOW BATTERY!");  // Software Low Batt alarm
      return;                      // Bail out
    }

  // prints assume 3 digit voltage
  if (usbPwr == 0) {             // No +5 from USB
    lcd.print(" Discharging ");
  } else {                       // USB is powering
    if (chgStat == 0) {          // Charger is active
      lcd.print(" Charging    ");
    } else {                     // Battery is fully charged
      lcd.print(" Charged     ");
    }
  }
}


/********************************************************
   function to read analog pins as debounced digital inputs
   Levels adjusted for 12 bit ADC in Teensy3.1
   works with 10k pull up and 0.01 or 0.1 ufd cap across switch
   has 33 percent hysteresis.
   Beware this will spin forever on an unterminated analog input
 ********************************************************/
boolean Button(int Bpin) {
  // For Teensy with analogResolution set to 12 bits 0-4095
  // Force an initial analogRead
  int level = 2048;              // middle of dead zone

  while (level > 1300 && level < 2700) { // spin if in dead zone
    level = analogRead(Bpin);    // try again
  }

  if (level >= 2700) {
    return HIGH;                 // in upper third
  }
  else {
    //   Serial.print(Bpin); Serial.println(" is low");
    return LOW;                  // in lower third
  }
}


/********************************************************
   Startup menu change functions
     setupMenu points to change type selected in setup
 ********************************************************/
void doSetupMenu(int setupMenu) {
  time_t Ltime;                  // Internal Unix time snapshot
  unsigned long TTime = 0;       // Millis target time
  // These six variables for parsing a Unix time_t
  unsigned int Lhour;            // Local hour for setting
  unsigned int Lmin;             // Local minute for setting
  unsigned int Lsec;             // Local second for setting
  unsigned int Lday;             // Local day for setting
  unsigned int Lmon;             // Local month for setting
  unsigned int Lyear;            // Local year for setting

  unsigned int cursorPos;        // LCD display digit being changed
  int Action;                    // Copy of upDown indicator
  bool ditToggle = false;        // Fake dit on/off flag
  bool Changed = false;          // Triggers clock update

  // ENTER was pressed to trigger entry into this routine so
  while (!Button(ENTER)) {}      // Spin wait for ENTER to release

  lcd.setCursor(0, 0);
  lcd.print("Changing        ");

  // Individual process for each setup() menu selection
  switch (setupMenu) {
    case 0:                      // Words Per Minute

      while (true) {             // Event loop
        Mtime = millis();        // Get current Elapsed Time

        // Morse code engine is not started yet so generate fake dits
        if (Mtime > currEltendtime) {   // Element time has expired
          dotLength = 1200 / wordsPerminute; // Set new element length
          currEltendtime = Mtime + dotLength; // Schedule element finish (millis)

          if (ditToggle) {
            sineTone(sideTone);  // tone on
          } else {
            nosineTone();        // tone off
          }
          ditToggle = !ditToggle; // Set up next Dit action
        }

        if (Mtime > TTime) {     // Time to check incr/decr
          TTime = Mtime + 200;   // Schedule the next update 5/second

          wordsPerminute += upDown();

          if (wordsPerminute > MAXWPM) wordsPerminute = MAXWPM; // range check
          if (wordsPerminute < MINWPM) wordsPerminute = MINWPM; // range check

          lcd.setCursor(0, 1);
          lcd.print(wordsPerminute);
          lcd.print(" WPM Speed    ");

          if (chkENTER() > LONGPRESS) { // Caught a Long press
            break;               // Exit while()
          }
        }
        waveShape();             // Manages the tone
      }                          // End case 0 event loop
      break;                     // Exit case 0

    case 1:                      // Sidetone Frequency

      while (true) {             // Event loop
        Mtime = millis();        // Get current Elapsed Time

        // Generate fake dits
        if (Mtime > currEltendtime) {    // Element time has expired
          dotLength = 1200 / wordsPerminute; // Set new element length
          currEltendtime = Mtime + dotLength; // Schedule element finish (millis)

          if (ditToggle) {
            sineTone(sideTone);  // tone on
          } else {
            nosineTone();        // tone off
          }
          ditToggle = !ditToggle;
        }

        if (Mtime > TTime) {     // Time to check incr/decr
          TTime = Mtime + 150;   // Schedule the next update

          sideTone += upDown() * 10;    // 10 Hz jumps
          if (sideTone > MAXTONE) sideTone = MAXTONE;  // range check
          if (sideTone < MINTONE) sideTone = MINTONE;  // range check

          lcd.setCursor(0, 1);
          lcdPz(sideTone, 4);
          lcd.print(" Tone Freq  ");

          if (chkENTER() > LONGPRESS) { // Caught a Long press
            break;               // Exit while()
          }
        }
        waveShape();             // Runs the side tone
      }                          // End case 1 event loop
      break;                     // Exit case 1

    case 2:                      // LCD backlight brightness

      while (true) {             // Spin while adjusting

        lcdBackLite += upDown() * 5;    // 5 count jumps
        if (lcdBackLite > MAXLCD) lcdBackLite = MAXLCD;
        if (lcdBackLite < MINLCD) lcdBackLite = MINLCD;

        lcd.setCursor(0, 1);
        lcdPz(lcdBackLite, 3);
        lcd.print(" LCD Backlite");
        analogWrite(BACKLITE, lcdBackLite);

        if (chkENTER() > LONGPRESS) {   // Caught a Long press
          break;                 // Exit while()
        }
        delay(100);              // Ten updates per second
      }
      break;                     // Exit case 2

    case 3:                      // GMT time

      Ltime = now();             // System time snapshot
      // Convert snapshot into simple variables for update
      Lhour = hour(Ltime);
      Lmin = minute(Ltime);
      Lsec = second(Ltime);

      lcd.setCursor(8, 1);
      lcd.print(" GMT   ");

      cursorPos = 0;             // start at second position
      lcd.cursor();

      lcd.setCursor(0, 1);

      // Walk the cursor down the displayed time
      while (true) {             // Event loop
        Action = upDown();       // Up or Down pressed?
        if (Action != 0) {
          delay(150);            // limit speed of change
          Changed = true;        // Flag something was changed
        }

        switch (cursorPos) {
          case 0:                // Skip Hours tens digit
            cursorPos += 1;
            break;
          case 1:                // Hours ones digit
            Lhour = (Lhour + Action) % 24;
            break;
          case 2:                // Ignore the colon
            cursorPos += 1;
            break;
          case 3:                // Skip Minutes tens digit
            cursorPos += 1;
            break;
          case 4:                // Minutes ones digit
            Lmin = (Lmin + Action) % 60;
            break;
          case 5:                // Ignore the colon
            cursorPos += 1;
            break;
          case 6:                // Skip Seconds tens digit
            cursorPos += 1;
            break;
          case 7:                // Seconds ones digit
            Lsec = (Lsec + Action) % 60;
            break;
        }

        if ((Mtime = chkENTER()) !=  0) { // Caught ENTER press
          cursorPos = (cursorPos + 1) % 7;
        }

        // A long ENTER press exits the time set routine
        if (Mtime > LONGPRESS) { // Caught a Long press
          if (Changed) {         // Only update if changed
            Ltime = now();       // Get fresh date dsnapshot
            Lday = day(Ltime);
            Lmon = month(Ltime);
            Lyear = year(Ltime);

            // Set system with changed time, unchanged date
            setTime(Lhour, Lmin, Lsec, Lday, Lmon, Lyear); //System
            Teensy3Clock.set(now());    // Push update to RTC
          }
          break;                 // Exit the while() loop
        }

        lcd.setCursor(0, 1);     // Update display
        lcdPz(Lhour, 2); lcd.print(":");// Formatted time
        lcdPz(Lmin, 2); lcd.print(":");
        lcdPz(Lsec, 2);
        lcd.setCursor(cursorPos, 1);    // move to next settable digit

        delay(10);               // Limit scan speed
      }                          // End event loop
      lcd.noCursor();            // Turn off the cursor
      break;                     // Exit case 3

    case 4:                      // GMT Date

      Ltime = now();             // Get a local snapshot
      // Convert time_t snapshot into simple variables for display
      Lday = day(Ltime);
      Lmon = month(Ltime);
      Lyear = year(Ltime);

      lcd.setCursor(10, 1);
      lcd.print(" GMT ");

      cursorPos = 0;             // start at first position
      lcd.cursor();

      lcd.setCursor(0, 1);

      // Walk the cursor down the displayed date
      while (true) {             // Event loop
        Action = upDown();       // Up or Down pressed?
        if (Action != 0) delay(150);    // limit speed of change
        switch (cursorPos) {
          case 0:                // Skip Month tens digit
            cursorPos += 1;
            break;
          case 1:                // Month ones digit
            Lmon = (Lmon + Action) % 12;
            if (Lmon < 1)Lmon = 12;     // there is no zero month
            break;
          case 2:                // Ignore the slash
            cursorPos += 1;
            break;
          case 3:                // Skip days tens digit
            cursorPos += 1;
            break;
          case 4:                // Days ones digit
            Lday = (Lday + Action) % 31;
            if (Lday < 1)Lday = 31;
            break;
          case 5:                // Ignore the slash
            cursorPos += 1;
            break;
          case 6:                // Skip Year high digits
            cursorPos += 1;
            break;
          case 7:                // Skip Year high digits
            cursorPos += 1;
            break;
          case 8:                // Skip Year high digits
            cursorPos += 1;
            break;
          case 9:                // Year ones digit
            Lyear = (Lyear + Action);
            if (Lyear > 2037) Lyear = 2037; // range check
            if (Lyear < 1970) Lyear = 1970; // range check
            break;
        }

        if ((Mtime = chkENTER()) !=  0) {  // Caught ENTER button press
          cursorPos = (cursorPos + 1) % 9; // rachet up cursor
        }

        // A long ENTER press exits the date set routine
        if (Mtime > LONGPRESS) { // Caught a Long press
          Ltime = now();         // Get a fresh snapshot
          Lhour = hour(Ltime);
          Lmin = minute(Ltime);
          Lsec = second(Ltime);

          // Update changed date and unchanged time
          setTime(Lhour, Lmin, Lsec, Lday, Lmon, Lyear);
          Teensy3Clock.set(now());      // Update the RTC
          break;                 // Exit the date set case
        }

        lcd.setCursor(0, 1);     // Update display
        lcdPz(Lmon, 2); lcd.print("/"); // Formatted
        lcdPz(Lday, 2); lcd.print("/");
        lcd.print(Lyear);
        lcd.setCursor(cursorPos, 1);    // move to next digit

        delay(10);               // Limit scan speed
      }                          // End case 4 event loop
      break;                     // Exit case 4

    case 5:                      // Set GMT offset
      while (true) {

        GMTOffset += upDown();
        if (GMTOffset > +12) GMTOffset = -12; // range check
        if (GMTOffset < -12) GMTOffset = +12; // range check

        lcd.setCursor(0, 1);
        lcd.print("    GMT Offset  ");
        lcd.setCursor(0, 1);
        if (GMTOffset >= 0) lcd.print("+");
        lcd.print(GMTOffset);

        if (chkENTER() > LONGPRESS) {   // Caught a Long press
          break;                 // Exit while()
        }
        delay(200);              // 5 updates per second
      }
      break;                     // Exit case 5

    case 6:                      // Reset to canned.h defaults
      // These four vars will be written at menu exit
      sideTone = DEFAULTTONE;
      wordsPerminute = DEFAULTSPEED;
      lcdBackLite = DEFAULTLCD;
      GMTOffset = GMTOFFSET;     // This one needs all 32 bits

      // Setup menu exit doesn't write message memory defaults so do it now
      for (int i = 0; i <= 6; i++) {    // Copy seven default memory strings
        for (int j = 0; j < MSGSIZE; j++) {  // Each char in string
          EEPROM.write(i * MSGSIZE + j + 8, cMessg[i][j]);
        }
      }

      Serial.println("Default Complete");
      lcd.setCursor(0, 1);
      lcd.print("Default Complete");
      beeps(LOWTONE);            // Announce default write is complete

      while (!Button(FUNCTION)) {} // Wait for func to be released
      break;                     // Exit case 6

    default:                     // Catch cases with no action
      lcd.setCursor(0, 1);
      lcd.print("Nothing Changed");
      Serial.println(setupMenu);
      break;
  }                              // Exit switch()

  lcd.noCursor();                // Turn off the cursor
}


/********************************************************
   Test ENTER button
    Returns zero if not pressed -or-
    Returns time pressed in milliseconds
    Replaces second line if long press
 ********************************************************/
unsigned long chkENTER() {
  unsigned long pressed;         // Time button was pressed (or zero if not)

  if (!Button(ENTER)) {          // ENTER button pressed
    pressed = millis();          // Mark entry time
    nosineTone();                // Tone off

    while (!Button(ENTER)) {     // Spin wait for release
      if ((Mtime = millis()) > (pressed + LONGPRESS)) {
        lcd.setCursor(0, 1);
        lcd.print("      EXIT      ");  // Tell user it's OK to let go
      }
      waveShape();               // Run the side tone while spinning
    }
    return (millis() - pressed); // Return millisecs time pressed
  } else {
    return 0L;                   // Return "was not pressed"
  }
}


/********************************************************
   Reads the designated Up/Down buttons.
   Returns
   +1 if up is pressed
   -1 if down is pressed
    0 if neither is pressed
 ********************************************************/
int upDown() {
  int returner = 0;
  if (!Button(UP)) {             // Increase value
    returner = 1;
  }
  if (!Button(DOWN)) {           // Decrease value
    returner = -1;
  }
  return returner;
}


/********************************************************
  Display the first 16 characters of a message memory on LDD
  Entire message is printed to serial
   arg is message number 0-6
 ********************************************************/
void showMem(int whichMem) {
  int j;
  char aByte;

  lcd.setCursor(0, 0);
  lcd.print("Stored Memory  ");
  lcd.print(whichMem + 1);       // Numbers msgs 1-7
  lcd.setCursor(0, 1);
  lcd.print("                "); // Erase last message
  lcd.setCursor(0, 1);
  Serial.print("Memory ");
  Serial.print(whichMem + 1);
  Serial.print(" \"");

  // Dig the message out of EEPROM
  for (j = 0; j < MSGSIZE; j++) {
    if ((aByte = EEPROM.read(whichMem * MSGSIZE + j + 8)) == 0) break;
    if (j <= 15) lcd.print(aByte); // Stop at 16 chars for LCD
    Serial.print(aByte);         // Serial gets the whole message
  }
  Serial.println("\"");
}


/********************************************************
  Read and average the battery voltage
  Set or clear the software Low Battery alarm flag
 ********************************************************/
void readBatt(void) {
  static bool firstRead = true;
  batVolts = analogRead(batVoltsPin);

  if (firstRead) {               // Initialize the average variable
    firstRead = false;
    batVoltsSmoothed = batVolts;
  }

  // Move the average by 5 percent of the error (14 bit resolution)
  batVoltsSmoothed = batVoltsSmoothed + (batVolts - batVoltsSmoothed) / 20;

  // Software check for low battery voltage
  if ((batVoltsSmoothed / 8) < LOWTRIGGER) { // Low voltage measured
    lowBattFlag = true;
  } else {
    lowBattFlag = false;
  }
}
