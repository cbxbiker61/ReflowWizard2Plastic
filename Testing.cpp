// Testing menu
// Called from the main loop
// Allows the user to test the outputs
// Buttons: The bottom button moves to the next output
//          The top button toggles the output on and off

#include <Arduino.h>
#include <ControLeo2.h>
#include "ReflowWizard.h"

namespace {

bool firstRun(true);
bool channelIsOn(true);
int channel(4);

void displayOnState(bool isOn)
{
	lcd.setCursor(9, 1);
	lcd.print(isOn ? "is on ": "is off");
}

} // namespace

// Called when in testing mode
// Return false to exit this mode
bool Testing(void)
{
	// Is this the first time "Testing" has been run?
	if ( firstRun )
	{
		firstRun = false;
		lcdPrintLineF(0, F("Test Outputs"));
		lcdPrintLineF(1, F("Output 4"));
		displayOnState(channelIsOn);
	}

	// Turn the currently selected channel on, and the others off
	for ( int i = 4; i < 8; ++i )
      digitalWrite(i, (i == channel && channelIsOn) ?  HIGH : LOW);

	// Was a button pressed?
	switch ( getButton() )
	{
	case CONTROLEO_BUTTON_TOP:
		// Toggle the output on and off
		channelIsOn = !channelIsOn;
		displayOnState(channelIsOn);
		break;
	case CONTROLEO_BUTTON_BOTTOM:
		++channel; // Move to the next output

		if ( channel == 8 )
		{
			// Turn all the outputs off
			for ( int i = 4; i < 8; ++i )
				digitalWrite(i, LOW);

			// Initialize variables for the next time through
			firstRun = true;
			channel = 4;
			channelIsOn = true;
			// Return to the main menu
			return false;
		}

		lcd.setCursor(7, 1);
		lcd.print(channel);
		displayOnState(channelIsOn);
		break;
	}

	return true; // Stay in this mode
}

