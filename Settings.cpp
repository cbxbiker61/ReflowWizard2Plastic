// The ATMEGA32U4 has 1024 bytes of EEPROM.  We use some of it to store settings so that
// the oven doesn't need to be reconfigured every time it is turned on.  Uninitialzed
// EEPROM is set to 0xFF (255).  One of the first things done when powering up is to
// see if the EEPROM is uninitialized - and initialize it if that is the case.
//
// All settings are stored as bytes (unsigned 8-bit values).  This presents a problem
// for the maximum temp which can be as high as 280C - which doesn't fit in a
// 8-bit variable.  With the goal of making getSetting/setSetting as simple as possible,
// we could've saved all values as 16-bit values, using consecutive EEPROM locations. We
// instead chose to just offset the temp by 150C, making the range 25 to 130 instead
// of 175 to 280.

#include <Arduino.h>
#include <EEPROM.h>
#include <ControLeo2.h>
#include "ReflowWizard.h"

void Settings::ensureInitialized(void)
{
	// Does the EEPROM need to be initialized?
	if ( get(EEPROM_NEEDS_INIT) )
	{
		// Initialize all the settings to 0 (false)
		for ( int i = 0; i < 1024; ++i )
			EEPROM.write(i, 0);

		set(MAX_TEMP, 240); // Set a reasonable max temp
		set(SERVO_CLOSED_DEGREES, 90); // Set the servos to neutral positions (90 degrees)
		set(SERVO_OPEN_DEGREES, 90);
		set(BAKE_TEMP, BAKE_MIN_TEMP); // Set default baking temp
	}

	// Legacy support - Initialize the rest of EEPROM for upgrade from 1.x to 1.4
	if ( get(SERVO_OPEN_DEGREES) > 180 )
	{
		for ( int i = SERVO_OPEN_DEGREES; i < 1024; ++i )
			EEPROM.write(i, 0);

		set(SERVO_CLOSED_DEGREES, 90);
		set(SERVO_OPEN_DEGREES, 90);
		set(BAKE_TEMP, BAKE_MIN_TEMP);
	}
}

// Get the setting from EEPROM
int Settings::get(int settingNum)
{
	int val(EEPROM.read(settingNum));

	// The maximum temp has an offset to allow it to be saved in 8-bits (0 - 255)
	if ( settingNum == MAX_TEMP )
		return val + TEMP_OFFSET;

	// Bake temp is modified before saving
	if ( settingNum == BAKE_TEMP )
		return val * BAKE_TEMP_STEP;

	return val;
}

// Save the setting to EEPROM.  EEPROM has limited write cycles, so don't save it if the
// value hasn't changed.
void Settings::set(int settingNum, int value)
{
	if ( get(settingNum) != value )
	{
		switch ( settingNum )
		{
		case D4_TYPE:
		case D5_TYPE:
		case D6_TYPE:
		case D7_TYPE:
			// The element has been reconfigured so reset the duty cycles and restart learning
			EEPROM.write(SETTINGS_CHANGED, true);
			EEPROM.write(LEARNING_MODE, true);
			EEPROM.write(settingNum, value);
			Serial.println(F("Settings changed!  Duty cycles have been reset and learning mode has been enabled"));
			break;

		case MAX_TEMP:
			// Enable learning mode if the maximum temp has changed a lot
			if ( abs(get(settingNum) - value) > 5 )
				EEPROM.write(LEARNING_MODE, true);

			// Write the new maximum temp
			EEPROM.write(settingNum, value - TEMP_OFFSET);
			break;

		case BAKE_TEMP:
			EEPROM.write(settingNum, value / BAKE_TEMP_STEP);
			break;

		default:
			EEPROM.write(settingNum, value);
			break;
		}
	}
}

