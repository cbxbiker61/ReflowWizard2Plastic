// This library is derived from Adafruit's I2C LiquidCrystal library.
// Adafruit provide excellent products and support for makers and tinkerers.
// Please support them from buying products from their web site.
// https://github.com/adafruit/LiquidCrystal/blob/master/LiquidCrystal.cpp
//
// This library controls the LCD display, the LCD backlight and the buzzer.  These
// are connected to the microcontroller using a I2C GPIO IC (MCP23008).
//
// Written by Peter Easton
// Released under WTFPL license
//
// Change History:
// 14 August 2014        Initial Version

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <Arduino.h>
#include "ControLeo2LiquidCrystal.h"

// Commands
#define CMD_CLEARDISPLAY 0x01
#define CMD_RETURNHOME 0x02
#define CMD_ENTRYMODESET 0x04
#define CMD_DISPLAYCONTROL 0x08
#define CMD_CURSORSHIFT 0x10
#define CMD_FUNCTIONSET 0x20
#define CMD_SETCGRAMADDR 0x40
#define CMD_SETDDRAMADDR 0x80

// Flags for display entry mode
#define FLG_ENTRYRIGHT 0x00
#define FLG_ENTRYLEFT 0x02
#define FLG_ENTRYSHIFTINCREMENT 0x01
#define FLG_ENTRYSHIFTDECREMENT 0x00

// Flags for display on/off control
#define FLG_DISPLAYON 0x04
#define FLG_DISPLAYOFF 0x00
#define FLG_CURSORON 0x02
#define FLG_CURSOROFF 0x00
#define FLG_BLINKON 0x01
#define FLG_BLINKOFF 0x00

// Flags for display/cursor shift
#define FLG_DISPLAYMOVE 0x08
#define FLG_CURSORMOVE 0x00
#define FLG_MOVERIGHT 0x04
#define FLG_MOVELEFT 0x00

// Flags for function set
#define FLG_8BITMODE 0x10
#define FLG_4BITMODE 0x00
#define FLG_2LINE 0x08
#define FLG_1LINE 0x00

namespace ControLeo2 {

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set:
//    DL = 1; 8-bit interface data
//    N = 0; 1-line display
//    F = 0; 5x8 dot character font
// 3. Display on/off control:
//    D = 0; Display off
//    C = 0; Cursor off
//    B = 0; Blinking off
// 4. Entry mode set:
//    I/D = 1; Increment by 1
//    S = 0; No shift
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// LiquidCrystal constructor is called).


LiquidCrystal::LiquidCrystal()
{
	_dspFunc = FLG_4BITMODE | FLG_1LINE | CHARSIZE_5x8DOTS;

	// Save the pins used to drive the LCD
	_rsPin = A0;
	_enPin = A1;
	_dataPins[0] = A2;
	_dataPins[1] = A3;
	_dataPins[2] = A4;
	_dataPins[3] = A5;

	// Set all the pins to be outputs
	pinMode(_rsPin, OUTPUT);
	pinMode(_enPin, OUTPUT);

	for ( int i = 0; i < 4; ++i )
		pinMode(_dataPins[i], OUTPUT);
}

void LiquidCrystal::begin(uint8_t cols, uint8_t lines, uint8_t dotsize)
{
	if ( lines > 1 )
        _dspFunc |= FLG_2LINE;

	_lineCount = lines;
	_curLine = 0;

	// For some 1 line displays you can select a 10 pixel high font
	if ( (dotsize != 0) && (lines == 1) )
		_dspFunc |= CHARSIZE_5x10DOTS;

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// According to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
	delayMicroseconds(50000);

	// Now we pull both RS and R/W low to begin commands
	digitalWrite(_rsPin, LOW);
	digitalWrite(_enPin, LOW);

	//Put the LCD into 4 bit mode
	// This is according to the hitachi HD44780 datasheet figure 24, pg 46
	// We start in 8bit mode, try to set 4 bit mode
	wr4Bits(0x03);
	delayMicroseconds(4500); // wait min 4.1ms

	// Second try
	wr4Bits(0x03);
	delayMicroseconds(4500); // wait min 4.1ms

	// Third go!
	wr4Bits(0x03);
	delayMicroseconds(150);

	// Finally, set to 8-bit interface
	wr4Bits(0x02);

	// Finally, set # lines, font size, etc.
	command(CMD_FUNCTIONSET | _dspFunc);

	// Turn the display on with no cursor or blinking default
	_dspCtl = FLG_DISPLAYON | FLG_CURSOROFF | FLG_BLINKOFF;
	display();

	// Clear the display
	clear();

	// Initialize to default text direction (for romance languages)
	_dspMode = FLG_ENTRYLEFT | FLG_ENTRYSHIFTDECREMENT;
	// Set the entry mode
	command(CMD_ENTRYMODESET | _dspMode);
}

void LiquidCrystal::clear(void)
{
	command(CMD_CLEARDISPLAY); // Clear display, set cursor position to zero
	delayMicroseconds(2000);   // This command takes a long time!
}

// Set the cursor position to (0, 0)
void LiquidCrystal::home(void)
{
	command(CMD_RETURNHOME); // Set cursor position to zero
	delayMicroseconds(2000); // This command takes a long time!
}

// Put the cursor in the specified position
void LiquidCrystal::setCursor(uint8_t col, uint8_t row)
{
	const int row_offsets[] = {0x00, 0x40, 0x14, 0x54};

	if ( row > _lineCount )
		row = _lineCount - 1;    // Count rows starting with 0

	command(CMD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void LiquidCrystal::noDisplay(void)
{
	_dspCtl &= ~FLG_DISPLAYON;
	command(CMD_DISPLAYCONTROL | _dspCtl);
}

void LiquidCrystal::display(void)
{
	_dspCtl |= FLG_DISPLAYON;
	command(CMD_DISPLAYCONTROL | _dspCtl);
}

// Turns the underline cursor on/off
void LiquidCrystal::noCursor(void)
{
	_dspCtl &= ~FLG_CURSORON;
	command(CMD_DISPLAYCONTROL | _dspCtl);
}

void LiquidCrystal::cursor(void)
{
	_dspCtl |= FLG_CURSORON;
	command(CMD_DISPLAYCONTROL | _dspCtl);
}

// Turn on and off the blinking cursor
void LiquidCrystal::noBlink(void)
{
	_dspCtl &= ~FLG_BLINKON;
	command(CMD_DISPLAYCONTROL | _dspCtl);
}

void LiquidCrystal::blink(void)
{
	_dspCtl |= FLG_BLINKON;
	command(CMD_DISPLAYCONTROL | _dspCtl);
}

// These commands scroll the display without changing the RAM
void LiquidCrystal::scrollDisplayLeft(void)
{
	command(CMD_CURSORSHIFT | FLG_DISPLAYMOVE | FLG_MOVELEFT);
}

void LiquidCrystal::scrollDisplayRight(void)
{
	command(CMD_CURSORSHIFT | FLG_DISPLAYMOVE | FLG_MOVERIGHT);
}

// This is for text that flows Left to Right
void LiquidCrystal::leftToRight(void)
{
	_dspMode |= FLG_ENTRYLEFT;
	command(CMD_ENTRYMODESET | _dspMode);
}

// This is for text that flows Right to Left
void LiquidCrystal::rightToLeft(void)
{
	_dspMode &= ~FLG_ENTRYLEFT;
	command(CMD_ENTRYMODESET | _dspMode);
}

// This will 'right justify' text from the cursor
void LiquidCrystal::autoscroll(void)
{
	_dspMode |= FLG_ENTRYSHIFTINCREMENT;
	command(CMD_ENTRYMODESET | _dspMode);
}

// This will 'left justify' text from the cursor
void LiquidCrystal::noAutoscroll(void)
{
	_dspMode &= ~FLG_ENTRYSHIFTINCREMENT;
	command(CMD_ENTRYMODESET | _dspMode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void LiquidCrystal::createChar(uint8_t location, uint8_t charmap[])
{
	location &= 0x7; // we only have 8 locations 0-7
	command(CMD_SETCGRAMADDR | (location << 3));

	for ( int i = 0; i < 8; ++i )
		write(charmap[i]);
}

size_t LiquidCrystal::write(uint8_t value)
{
	send(value, HIGH);
	return 1;
}

// Low level data pushing commands
// Write either command or data, with automatic 4/8-bit selection
void LiquidCrystal::send(uint8_t value, uint8_t mode)
{
	digitalWrite(_rsPin, mode);

	wr4Bits(value>>4);
	wr4Bits(value);
}

void LiquidCrystal::wr4Bits(uint8_t value)
{
	for ( int i = 0; i < 4; ++i )
		digitalWrite(_dataPins[i], (value >> i) & 0x01);

	// Pulse enable
	digitalWrite(_enPin, LOW);
	delayMicroseconds(1);
	digitalWrite(_enPin, HIGH);
	delayMicroseconds(1);    // enable pulse must be >450ns
	digitalWrite(_enPin, LOW);
	delayMicroseconds(100);   // commands need > 37us to settle
}

} // namespace ControLeo2

