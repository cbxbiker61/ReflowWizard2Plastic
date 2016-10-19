#pragma once

#include <ControLeo2.h>

#include <avr/pgmspace.h>

// Output type
#define TYPE_UNUSED         0
#define TYPE_TOP_ELEMENT    1
#define TYPE_BOTTOM_ELEMENT 2
#define TYPE_BOOST_ELEMENT  3
#define TYPE_CONVECTION_FAN 4
#define TYPE_COOLING_FAN    5
#define NO_OF_TYPES         6
#define isHeatingElement(x) (x == TYPE_TOP_ELEMENT || x == TYPE_BOTTOM_ELEMENT || x == TYPE_BOOST_ELEMENT)

#define TEMP_OFFSET    150 // To allow temp to be saved in 8-bits (0-255)
#define BAKE_TEMP_STEP   5 // Allows the storing of the temp range in one byte
#define BAKE_MAX_DURATION     176 // 176 = 18 hours (see getBakeSeconds)
#define BAKE_MIN_TEMP   40 // Minimum temp for baking
#define BAKE_MAX_TEMP  200 // Maximum temp for baking

extern ControLeo2::LiquidCrystal lcd;

class Settings
{
public:

// EEPROM settings
// Remember that EEPROM initializes to 0xFF after flashing the bootloader
//
	enum {
		EEPROM_NEEDS_INIT = 0
		, D4_TYPE // Element type controlled by D4 (or fan, unused)
		, D5_TYPE // Element type controlled by D5 (or fan, unused)
		, D6_TYPE // Element type controlled by D6 (or fan, unused)
		, D7_TYPE // Element type controlled by D7 (or fan, unused)
		, MAX_TEMP // Max oven temp.  Reflow curve will be based on this (stored temp is offset by 150 degrees)
		, SETTINGS_CHANGED // Settings have changed.  Relearn duty cycles
		, BAKE_TEMP // The baking temp (divided by 5)
		, BAKE_DURATION // The baking duration (see getBakeSeconds)

		// Learned settings
		, LEARNING_MODE = 10   // ControLeo is learning oven response and will make adjustments
		, PRESOAK_D4_DUTY_CYCLE // Duty cycle (0-100) that D4 must be used during presoak
		, PRESOAK_D5_DUTY_CYCLE // Duty cycle (0-100) that D5 must be used during presoak
		, PRESOAK_D6_DUTY_CYCLE // Duty cycle (0-100) that D6 must be used during presoak
		, PRESOAK_D7_DUTY_CYCLE // Duty cycle (0-100) that D7 must be used during presoak
		, SOAK_D4_DUTY_CYCLE // Duty cycle (0-100) that D4 must be used during soak
		, SOAK_D5_DUTY_CYCLE // Duty cycle (0-100) that D5 must be used during soak
		, SOAK_D6_DUTY_CYCLE // Duty cycle (0-100) that D6 must be used during soak
		, SOAK_D7_DUTY_CYCLE // Duty cycle (0-100) that D7 must be used during soak
		, REFLOW_D4_DUTY_CYCLE // Duty cycle (0-100) that D4 must be used during reflow
		, REFLOW_D5_DUTY_CYCLE // Duty cycle (0-100) that D4 must be used during reflow
		, REFLOW_D6_DUTY_CYCLE // Duty cycle (0-100) that D4 must be used during reflow
		, REFLOW_D7_DUTY_CYCLE // Duty cycle (0-100) that D4 must be used during reflow
		, SERVO_OPEN_DEGREES // The position the servo should be in when the door is open
		, SERVO_CLOSED_DEGREES // The position the servo should be in when the door is closed
	};

	static void ensureInitialized(void);
	static int get(int settingNum);
	static void set(int settingNum, int value);
};

class Tunes
{
public:
	enum {
		STARTUP
		, TOP_BUTTON_PRESS
		, BOTTOM_BUTTON_PRESS
		, REFLOW_COMPLETE
		, REMOVE_BOARDS
	};

	static void playStartup(void) { play(STARTUP); }
	static void playTopButtonPress(void) { play(TOP_BUTTON_PRESS); }
	static void playBottomButtonPress(void) { play(BOTTOM_BUTTON_PRESS); }
	static void playReflowComplete(void) { play(REFLOW_COMPLETE); }
	static void playRemoveBoards(void) { play(REMOVE_BOARDS); }

private:
	static void play(int tune);
};

void initializeTimer(void);
bool Config(void);
bool Reflow(void);
bool Testing(void);
bool Bake(void);
bool RefreshPla(void);
bool DryPla(void);
bool DryAbs(void);
bool DryPetg(void);
bool DryNylon(void);
bool DryDesiccant(void);

int getButton(void);
uint32_t getBakeSeconds(int duration);
int getCurrentTemp(double &target); // 0 = success, 1 = open fault, 2 = short to gnd, 3 = short to vcc
void displayTemp(void);
void displayTemp(double);

void setServoPosition(unsigned int servoDegrees, int timeToTake);

void displayDuration(int offset, uint32_t duration);
void displayMaxTemp(int);
void displayServoDegrees(int degrees);

void lcdPrintLine(int line, const char *str);
void lcdPrintLineF(int line, const __FlashStringHelper *, int leadingSpaces = 0);

void takeCurrentThermocoupleReading(void);

