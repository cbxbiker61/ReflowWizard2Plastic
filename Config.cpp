#include "ReflowWizard.h"

// Setup menu
// Called from the main loop
// Allows the user to configure the outputs and maximum temp

const char *outputDesc[NO_OF_TYPES] = {"Unused", "Top", "Bottom", "Boost", "Convection Fan", "Cooling Fan"};

namespace {

int setupPhase;
int output = 4;    // Start with output D4
int type = TYPE_TOP_ELEMENT;
bool drawMenu = true;
int maxTemp;
int servoDegrees;
int servoDegreesIncrement = 5;
int selectedServo = Settings::SERVO_OPEN_DEGREES;
int bakeTemp;
int bakeDuration;

void setupOutputs(void)
{
	if ( drawMenu )
	{
		drawMenu = false;
		lcdPrintLineF(0, F("Dx is"));
		lcd.setCursor(1, 0);
		lcd.print(output);
		type = Settings::get(Settings::D4_TYPE - 4 + output);
		lcdPrintLine(1, outputDesc[type]);
	}

	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		type = (type+1) % NO_OF_TYPES; // Move to the next type
		lcdPrintLine(1, outputDesc[type]);
		break;

	case CONTROLEO_BUTTON_BOTTOM:
		Settings::set(Settings::D4_TYPE - 4 + output, type);
		++output;

		if ( output != 8 )
		{
			lcd.setCursor(1, 0);
			lcd.print(output);
			type = Settings::get(Settings::D4_TYPE - 4 + output);
			lcdPrintLine(1, outputDesc[type]);
			break;
		}

		++setupPhase;
		output = 4;
		break;
	}
}

void getMaxTemp(void)
{
	if ( drawMenu )
	{
		drawMenu = false;
		lcdPrintLineF(0, F("Max temp"));
		lcdPrintLineF(1, F("xxx\1C"));
		maxTemp = Settings::get(Settings::MAX_TEMP);
		displayMaxTemp(maxTemp);
	}

	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		++maxTemp;

		if ( maxTemp > 280 )
			maxTemp = 175;

		displayMaxTemp(maxTemp);
		break;
	case CONTROLEO_BUTTON_BOTTOM:
		Settings::set(Settings::MAX_TEMP, maxTemp);
		++setupPhase;
	}
}

void getServoSettings(void)
{
	if ( drawMenu )
	{
		drawMenu = false;
		lcdPrintLineF(0, F("Door servo"));
		lcdPrintLineF(1, (selectedServo == Settings::SERVO_OPEN_DEGREES)
							? F("open:")
							: F("closed:"));
		servoDegrees = Settings::get(selectedServo);
		displayServoDegrees(servoDegrees);
		// Move the servo to the saved position
		setServoPosition(servoDegrees, 1000);
	}

	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		// Should the servo increment change direction?
		if ( servoDegrees >= 180 )
			servoDegreesIncrement = -5;
		else if ( servoDegrees == 0 )
			servoDegreesIncrement = 5;

		// Change the servo angle
		servoDegrees += servoDegreesIncrement;
		// Move the servo to this new position
		setServoPosition(servoDegrees, 200);
		displayServoDegrees(servoDegrees);
		break;

	case CONTROLEO_BUTTON_BOTTOM:
		Settings::set(selectedServo, servoDegrees);

		// Go to the next phase.  Reset variables used in this phase
		if ( selectedServo == Settings::SERVO_OPEN_DEGREES )
		{
			selectedServo = Settings::SERVO_CLOSED_DEGREES;
			drawMenu = true;
		}
		else
		{
			selectedServo = Settings::SERVO_OPEN_DEGREES;
			++setupPhase;
		}
	}
}

void getBakeTemp(void)
{
	if ( drawMenu )
	{
		drawMenu = false;
		lcdPrintLineF(0, F("Bake temp"));
		lcdPrintLine(1, "");
		bakeTemp = Settings::get(Settings::BAKE_TEMP);
		lcd.setCursor(0, 1);
		lcd.print(bakeTemp);
		lcd.print("\1C ");
	}

	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		bakeTemp += BAKE_TEMP_STEP;

		if ( bakeTemp > BAKE_MAX_TEMP )
			bakeTemp = BAKE_MIN_TEMP;

		lcd.setCursor(0, 1);
		lcd.print(bakeTemp);
		lcd.print("\1C ");
		break;

	case CONTROLEO_BUTTON_BOTTOM:
		Settings::set(Settings::BAKE_TEMP, bakeTemp);
		++setupPhase;
	}
}

void getBakeDuration(void)
{
	if ( drawMenu )
	{
		drawMenu = false;
		lcdPrintLineF(0, F("Bake duration"));
		lcdPrintLine(1, "");
		bakeDuration = Settings::get(Settings::BAKE_DURATION);
		displayDuration(0, getBakeSeconds(bakeDuration));
	}

	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		// Increase the duration
		++bakeDuration %= BAKE_MAX_DURATION;
		displayDuration(0, getBakeSeconds(bakeDuration));
		break;

	case CONTROLEO_BUTTON_BOTTOM:
		Settings::set(Settings::BAKE_DURATION, bakeDuration);
		++setupPhase;
		break;
	}
}

void restartLearning(void)
{
	if ( drawMenu )
	{
		drawMenu = false;

		if ( ! Settings::get(Settings::LEARNING_MODE) )
		{
			lcdPrintLineF(0, F("Restart learning"));
			lcdPrintLineF(1, F("mode?      No ->"));
		}
		else
		{
			lcdPrintLineF(0, F("Oven is in"));
			lcdPrintLineF(1, F("learning mode"));
		}
	}

	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		Settings::set(Settings::LEARNING_MODE, true);
		drawMenu = true;
		break;

	case CONTROLEO_BUTTON_BOTTOM:
		++setupPhase;
		break;
	}
}

void restoreFactory(void)
{
	if ( drawMenu )
	{
		drawMenu = false;
		lcdPrintLineF(0, F("Restore factory"));
		lcdPrintLineF(1, F("settings?  No ->"));
	}

	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		lcdPrintLineF(0, F("Please wait ..."));
		lcdPrintLine(1, "");
		Settings::set(Settings::EEPROM_NEEDS_INIT, true);
		Settings::ensureInitialized();
		break;

	case CONTROLEO_BUTTON_BOTTOM:
		++setupPhase;
		break;
	}
}

} // namespace

// Called when in setup mode
// Return false to exit this mode
bool Config(void)
{
	int oldSetupPhase = setupPhase;

	switch ( setupPhase )
	{
	case 0: setupOutputs(); break;
	case 1: getMaxTemp(); break;
	case 2: getServoSettings(); break;
	case 3: getBakeTemp(); break;
	case 4: getBakeDuration(); break;
	case 5: restartLearning(); break;
	case 6: restoreFactory(); break;
	default: break;
	}

	// Does the menu option need to be redrawn?
	if ( oldSetupPhase != setupPhase )
		drawMenu = true;

	if ( setupPhase > 6 )
	{
		setupPhase = 0;
		return false;
	}

	return true;
}

void displayMaxTemp(int temp)
{
	lcd.setCursor(0, 1);
	lcd.print(temp);
}

void displayServoDegrees(int degrees)
{
	lcd.setCursor(8, 1);
	lcd.print(degrees);
	lcd.print("\1 ");
}

void displayDuration(int offset, uint32_t duration)
{
	lcd.setCursor(offset, 1);

	if ( duration >= 3600 )
	{
		lcd.print((duration / 3600));
		lcd.print("h ");
	}

	lcd.print((duration % 3600) / 60);
	lcd.print("m ");

	if ( duration < 3600 )
	{
		lcd.print(duration % 60);
		lcd.print("s ");
	}

	lcd.print("   ");
}

