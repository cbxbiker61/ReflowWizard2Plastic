#pragma once

// Written by Peter Easton
// Released under WTFPL license
//
// Change History:
// 14 August 2014        Initial Version

#include <Arduino.h>
#include <Print.h>
#include <stdint.h>

namespace ControLeo2 {

class LiquidCrystal : public Print
{
public:
	LiquidCrystal();

	enum {
		CHARSIZE_5x8DOTS = 0x00
		, CHARSIZE_5x10DOTS = 0x04
	};

	void begin(uint8_t cols, uint8_t rows, uint8_t charsize = CHARSIZE_5x8DOTS);
	void clear(void);
	void home(void);
	void setCursor(uint8_t, uint8_t);
	void noDisplay(void);
	void display(void);
	void noCursor(void);
	void cursor(void);
	void noBlink(void);
	void blink(void);
	void scrollDisplayLeft(void);
	void scrollDisplayRight(void);
	void leftToRight(void);
	void rightToLeft(void);
	void autoscroll(void);
	void noAutoscroll(void);

	void createChar(uint8_t, uint8_t[]);

	virtual size_t write(uint8_t value);
	void command(uint8_t value) { send(value, LOW); }

private:
	void send(uint8_t, uint8_t);
	void wr4Bits(uint8_t);

	uint8_t _rsPin; // LOW: command.  HIGH: character.
	uint8_t _enPin; // Activated by a HIGH pulse.
	uint8_t _dataPins[8];
	uint8_t _dspFunc;
	uint8_t _dspCtl;
	uint8_t _dspMode;
	uint8_t _lineCount;
	uint8_t _curLine;
};

} // namespace ControLeo2

