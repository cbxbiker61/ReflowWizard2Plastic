// Reflow logic
// Called from the main loop 20 times per second
// This where the reflow logic is controlled

#include <Arduino.h>
#include "ReflowWizard.h"

#define MILLIS_TO_SECONDS ((long) 1000)

extern const char *outputDesc[];

namespace {

// The data for each of pre-soak, soak and reflow phases
struct phaseData
{
	int elementDutyCycle[4]; // Duty cycle for each output
	int endTemp;             // The temp at which to move to the next reflow phase
	int phaseMinDuration;    // The minimum number of seconds that this phase should run for
	int phaseMaxDuration;    // The maximum number of seconds that this phase should run for
};

// Phases of reflow
#define PHASE_INIT               0 // Variable initialization
#define PHASE_PRESOAK            1 // Pre-soak rapidly gets the oven to around 150C
#define PHASE_SOAK               2 // Soak brings the PCB and components to the same (high) temp
#define PHASE_REFLOW             3 // Reflow melts the solder
#define PHASE_WAITING            4 // After reaching max temp, wait a bit for heat to permeate and start cooling
#define PHASE_COOLING_BOARDS_IN  5 // The oven door should be open and the boards remain in until components definitely won't move
#define PHASE_COOLING_BOARDS_OUT 6 // Boards can be removed. Remain in this state until another reflow can be started at 50C
#define PHASE_ABORT_REFLOW       7 // The reflow was aborted or completed.

const char NULL_STR[] = "";
const char PRESOAK_STR[] = "Presoak";
const char SOAK_STR[] = "Soak";
const char REFLOW_STR[] = "Reflow";
const char WAITING_STR[] = "Waiting";
const char COOLING_STR[] = "Cooling";
const char COOL_OPEN_DOOR_STR[] = "Cool - open door";
const char ABORT_STR[] = "Abort";

const char *phaseDesc[] = {NULL_STR, PRESOAK_STR, SOAK_STR, REFLOW_STR, WAITING_STR, COOLING_STR, COOL_OPEN_DOOR_STR, ABORT_STR};

int reflowPhase(PHASE_INIT);
int outputType[4];
int maxTemp;
bool learningMode;
phaseData phase[PHASE_REFLOW+1];
unsigned long phaseStartTime;
unsigned long reflowStartTime;
int elementDutyCounter[4];
int counter(0);
bool firstTimeInPhase(true);

// Print data about the phase to the serial port
void serialDisplayPhaseData(int phase, struct phaseData *pd, int *outputType)
{
	char buf[80];

	snprintf(buf, sizeof(buf), "******* Phase: %s *******", phaseDesc[phase]);
	Serial.println(buf);

	snprintf(buf, sizeof(buf), "Min duration = %d seconds", pd->phaseMinDuration);
	Serial.println(buf);

	snprintf(buf, sizeof(buf), "Max duration = %d seconds", pd->phaseMaxDuration);
	Serial.println(buf);

	snprintf(buf, sizeof(buf), "End temp = %d Celsius", pd->endTemp);
	Serial.println(buf);

	Serial.println(F("Duty cycles: "));

	for (int i = 0; i < 4; ++i )
	{
		snprintf(buf, sizeof(buf), "  D%d = %d  (%s)"
				, i + 4
				, pd->elementDutyCycle[i]
				, outputDesc[outputType[i]]);
		Serial.println(buf);
	}
}

void thermocoupleFault(int fault)
{
	lcdPrintLineF(0, F("Thermocouple err"));
	Serial.print(F("Thermocouple Error: "));

	switch ( fault )
	{
	case 1:
		lcdPrintLineF(1, F("Fault open"));
		Serial.println(F("Fault open"));
		break;
	case 2:
		lcdPrintLineF(1, F("Short to GND"));
		Serial.println(F("Short to ground"));
		break;
	case 3:
		lcdPrintLineF(1, F("Short to VCC"));
		break;
	}

	// Abort the reflow
	Serial.println(F("Reflow aborted because of thermocouple error!"));
	reflowPhase = PHASE_ABORT_REFLOW;
}

// Display the current temp to the LCD screen and print it to the serial port so it can be plotted
void displayReflowTemp(unsigned long currentTime
		, unsigned long startTime
		, unsigned long phaseTime
		, double temp)
{
	// Display the temp on the LCD screen
	displayTemp(temp);

	// Write the time and temp to the serial port, for graphing or analysis on a PC
	char buf[80];

	snprintf(buf, sizeof(buf), "%ld, %ld, "
			, (currentTime - startTime) / MILLIS_TO_SECONDS
			, (currentTime - phaseTime) / MILLIS_TO_SECONDS);
	Serial.print(buf);
	Serial.println(temp);
}

// Displays a message like "Reflow:Too slow"
void lcdPrintPhaseMessage(int phase, const char *str)
{
	if ( str && strlen(str) <= 8 )
	{
		char buf[20];
		snprintf(buf, sizeof(buf), "%s:%s", phaseDesc[phase], str);
		lcdPrintLine(0, buf);
	}
}

// Convenience function to save flash and RAM space
void displayAdjustmentsMadeContinue(bool willContinue)
{
	Serial.println(F("Adjustments have been made to duty cycles for this phase. "));
	Serial.println(willContinue ? F("Continuing ...") : F("Aborting ..."));
}

// Adjust the duty cycle for all elements by the given adjustment value
void adjustPhaseDutyCycle(int phase, int adjustment)
{
	int newDutyCycle;
	char buf[80];

	snprintf(buf, sizeof(buf)
			, "Adjusting duty cycles for %s phase by %d"
			, phaseDesc[phase], adjustment);
	Serial.println(buf);

	// Loop through the 4 outputs
	for ( int i = 0; i < 4; ++i )
	{
		int dutySetting(Settings::PRESOAK_D4_DUTY_CYCLE + ((phase-1) * 4) + i);
		newDutyCycle = Settings::get(dutySetting) + adjustment;
		newDutyCycle = constrain(newDutyCycle, 0, 100); // Duty cycle must be between 0 and 100%

		switch( Settings::get(Settings::D4_TYPE + i) )
		{
		case TYPE_BOOST_ELEMENT:
			// To avoid overstressing the boost element (which is just a mold heater),
			// don't allow more than a 60% duty cycle
			newDutyCycle = constrain(newDutyCycle, 0, 60);
			// Fall through ...
		case TYPE_TOP_ELEMENT:
		case TYPE_BOTTOM_ELEMENT:
			snprintf(buf, sizeof(buf), "D%d (%s) changed from %d to %d"
							, i + 4
							, outputDesc[Settings::get(Settings::D4_TYPE + i)]
							, Settings::get(dutySetting), newDutyCycle);
			Serial.println(buf);
			Settings::set(dutySetting, newDutyCycle); // Save the new duty cycle
			break;

		default:
			// Don't change the duty cycle if the output is unused, or used for a convection fan
			break;
		}
	}
}

void abortReflow(void)
{
	reflowPhase = PHASE_ABORT_REFLOW;
	lcdPrintLineF(0, F("Aborting reflow"));
	lcdPrintLineF(1, F("Button pressed"));
	Serial.println(F("Button pressed.  Aborting reflow ..."));
	delay(2000);
}

void phaseInit(int &elementDutyStart, const double currentTemp)
{
	// Make sure the oven is cool.  This makes for more predictable/reliable reflows and
	// gives the SSR's time to cool down a bit.
	if ( currentTemp > 50.0 )
	{
		lcdPrintLineF(0, F("Temp > 50\1C"));
		lcdPrintLineF(1, F("Please wait..."));
		Serial.println(F("Oven too hot to start reflow.  Please wait ..."));

		// Abort the reflow
		reflowPhase = PHASE_ABORT_REFLOW;
		return;
	}

	// Get the types for the outputs (elements, fan or unused)
	for ( int i = 0; i < 4; ++i )
		outputType[i] = Settings::get(Settings::D4_TYPE + i);

	// Get the maximum temp
	maxTemp = Settings::get(Settings::MAX_TEMP);

	// If the settings have changed then set up learning mode
	if ( Settings::get(Settings::SETTINGS_CHANGED) == true )
	{
		Settings::set(Settings::SETTINGS_CHANGED, false);
		// Tell the user that learning mode is being enabled
		lcdPrintLineF(0, F("Settings changed"));
		lcdPrintLineF(1, F("Initializing..."));
		Serial.println(F("Settings changed by user.  Reinitializing element duty cycles and enabling learning mode ..."));

		// Turn learning mode on
		Settings::set(Settings::LEARNING_MODE, true);
		// Set the starting duty cycle for each output.  These settings are conservative
		// because it is better to increase them each cycle rather than risk damage to
		// the PCB or components
		// Presoak = Rapid rise in temp
		//           Lots of heat from the bottom, some from the top
		// Soak    = Stabilize temp of PCB and components
		//           Heat from the bottom, not much from the top
		// Reflow  = Heat the solder and pads rapidly
		//           Lots of heat from all directions

		for ( int i = 0; i < 4; ++i )
		{
			switch ( outputType[i] )
			{
			case TYPE_UNUSED:
			case TYPE_COOLING_FAN:
				Settings::set(Settings::PRESOAK_D4_DUTY_CYCLE + i, 0);
				Settings::set(Settings::SOAK_D4_DUTY_CYCLE + i, 0);
				Settings::set(Settings::REFLOW_D4_DUTY_CYCLE + i, 0);
				break;
			case TYPE_TOP_ELEMENT:
				Settings::set(Settings::PRESOAK_D4_DUTY_CYCLE + i, 50);
				Settings::set(Settings::SOAK_D4_DUTY_CYCLE + i, 40);
				Settings::set(Settings::REFLOW_D4_DUTY_CYCLE + i, 50);
				break;
			case TYPE_BOTTOM_ELEMENT:
				Settings::set(Settings::PRESOAK_D4_DUTY_CYCLE + i, 80);
				Settings::set(Settings::SOAK_D4_DUTY_CYCLE + i, 70);
				Settings::set(Settings::REFLOW_D4_DUTY_CYCLE + i, 80);
				break;
			case TYPE_BOOST_ELEMENT:
				Settings::set(Settings::PRESOAK_D4_DUTY_CYCLE + i, 30);
				Settings::set(Settings::SOAK_D4_DUTY_CYCLE + i, 35);
				Settings::set(Settings::REFLOW_D4_DUTY_CYCLE + i, 50);
				break;
			case TYPE_CONVECTION_FAN:
				Settings::set(Settings::PRESOAK_D4_DUTY_CYCLE + i, 100);
				Settings::set(Settings::SOAK_D4_DUTY_CYCLE + i, 100);
				Settings::set(Settings::REFLOW_D4_DUTY_CYCLE + i, 100);
				break;
			}
		}

		// Wait a bit to allow the user to read the message
		delay(3000);
	} // end of settings changed

	// Read all the settings
	learningMode = Settings::get(Settings::LEARNING_MODE);

	for ( int i = PHASE_PRESOAK; i <= PHASE_REFLOW; ++i )
	{
		for ( int j = 0; j < 4; ++j )
			phase[i].elementDutyCycle[j] = Settings::get(Settings::PRESOAK_D4_DUTY_CYCLE + ((i-PHASE_PRESOAK) *4) + j);

		// Time to peak temp should be between 3.5 and 5.5 minutes.
		// While J-STD-20 gives exact phase temps, the reading depends very much on the thermocouple used
		// and its location.  Varying the phase temps as the max temp changes allows for thermocouple
		// variation.
		// Keep in mind that there is a 2C error in the MAX31855, and typically a 3C error in the thermocouple.
		switch ( i )
		{
		case PHASE_PRESOAK:
			phase[i].endTemp = maxTemp * 3 / 5; // J-STD-20 gives 150C
			phase[i].phaseMinDuration = 60;
			phase[i].phaseMaxDuration = 100;
			break;
		case PHASE_SOAK:
			phase[i].endTemp = maxTemp * 4 / 5; // J-STD-20 gives 200C
			phase[i].phaseMinDuration = 80;
			phase[i].phaseMaxDuration = 140;
			break;
		case PHASE_REFLOW:
			phase[i].endTemp = maxTemp;
			phase[i].phaseMinDuration = 60;
			phase[i].phaseMaxDuration = 100;
			break;
		}
	}

	// Let the user know if learning mode is on
	if ( learningMode )
	{
		lcdPrintLineF(0, F("Learning Mode"));
		lcdPrintLineF(1, F("is enabled"));
		Serial.println(F("Learning mode is enabled.  Duty cycles may be adjusted automatically if necessary"));
		delay(3000);
	}

	// Move to the next phase
	reflowPhase = PHASE_PRESOAK;
	lcdPrintLine(0, phaseDesc[reflowPhase]);
	lcdPrintLine(1, "");

	// Display information about this phase
	serialDisplayPhaseData(reflowPhase, &phase[reflowPhase], outputType);

	// Stagger the element start cycle to avoid abrupt changes in current draw
	for ( int i = 0; i < 4; ++i )
	{
		elementDutyCounter[i] = elementDutyStart;
		// Turn the next element on (elementDutyCounter[i+1] == 0)
		// when this element turns off (elementDutyCounter[i] == phase[reflowPhase].elementDutyCycle[i])
		// For example, assume two element both at 20% duty cycle.
		//   The counter for the first starts at 0
		//   The counter for the second should start at 80,
		//   because by the time counter 1 = 20 (1 turned off) counter 2 = 0 (counter 2 turned on)
		elementDutyStart = (100 + elementDutyStart - phase[reflowPhase].elementDutyCycle[i]) % 100;
	}

	// Start the reflow and phase timers
	reflowStartTime = millis();
	phaseStartTime = reflowStartTime;
}

void phaseHeat(int &elementDutyStart, const double currentTemp, const unsigned long currentTime)
{
	// Has the ending temp for this phase been reached?
	if ( currentTemp >= phase[reflowPhase].endTemp )
	{
		// Was enough time spent in this phase?
		if ( currentTime - phaseStartTime < (unsigned long) (phase[reflowPhase].phaseMinDuration * MILLIS_TO_SECONDS) )
		{
			char buf[80];

			snprintf(buf, sizeof(buf), "Warning: Oven heated up too quickly! Phase took %ld seconds."
					, (currentTime - phaseStartTime) / MILLIS_TO_SECONDS);
			Serial.println(buf);

			// Too little time was spent in this phase
			if ( learningMode )
			{
				// Were the settings close to being right for this phase?  Within 5 seconds?
				if ( phase[reflowPhase].phaseMinDuration - ((currentTime - phaseStartTime) / 1000) < 5 )
				{
					// Reduce the duty cycle for the elements for this phase, but continue with this run
					adjustPhaseDutyCycle(reflowPhase, -4);
					displayAdjustmentsMadeContinue(true);
				}
				else
				{
					// The oven heated up way too fast
					adjustPhaseDutyCycle(reflowPhase, -7);

					// Abort this run
					lcdPrintPhaseMessage(reflowPhase, "Too fast");
					lcdPrintLineF(1, F("Aborting ..."));
					reflowPhase = PHASE_ABORT_REFLOW;

					displayAdjustmentsMadeContinue(false);
					return;
				}
			}
			else
			{
				// It is bad to make adjustments when not in learning mode, because this leads to inconsistent
				// results.  However, this situation cannot be ignored.  Reduce the duty cycle slightly but
				// don't abort the reflow
				adjustPhaseDutyCycle(reflowPhase, -1);
				Serial.println(F("Duty cycles lowered slightly for future runs"));
			}
		}

		// The temp is high enough to move to the next phase
		++reflowPhase;
		firstTimeInPhase = true;
		lcdPrintLine(0, phaseDesc[reflowPhase]);
		phaseStartTime = millis();

		// Stagger the element start cycle to avoid abrupt changes in current draw
		for ( int i = 0; i < 4; ++i )
		{
			elementDutyCounter[i] = elementDutyStart;
			// Turn the next element on (elementDutyCounter[i+1] == 0)
			// when this element turns off (elementDutyCounter[i] == phase[reflowPhase].elementDutyCycle[i])
			// For example, assume two element both at 20% duty cycle.
			//   The counter for the first starts at 0
			//   The counter for the second should start at 80,
			//   because by the time counter 1 = 20 (1 turned off) counter 2 = 0 (counter 2 turned on)
			elementDutyStart = (100 + elementDutyStart - phase[reflowPhase].elementDutyCycle[i]) % 100;
		}

		// Display information about this phase
		if ( reflowPhase <= PHASE_REFLOW )
			serialDisplayPhaseData(reflowPhase, &phase[reflowPhase], outputType);

		return;
	}

	// Has too much time been spent in this phase?
	if ( currentTime - phaseStartTime > (unsigned long) (phase[reflowPhase].phaseMaxDuration * MILLIS_TO_SECONDS))
	{
		Serial.print(F("Warning: Oven heated up too slowly! Current temp is "));
		Serial.println(currentTemp);

		// Still in learning mode?
		if ( learningMode )
		{
			double tempDelta(phase[reflowPhase].endTemp - currentTemp);

			if ( tempDelta <= 3 )
			{
				// Almost made it!  Make a small adjustment to the duty cycles.  Continue with the reflow
				adjustPhaseDutyCycle(reflowPhase, 4);
				displayAdjustmentsMadeContinue(true);
				phase[reflowPhase].phaseMaxDuration += 15;
			}
			else
			{
				// A more dramatic temp increase is needed for this phase
				if ( tempDelta < 10 )
					adjustPhaseDutyCycle(reflowPhase, 8);
				else
					adjustPhaseDutyCycle(reflowPhase, 15);

				// Abort this run
				lcdPrintPhaseMessage(reflowPhase, "Too slow");
				lcdPrintLineF(1, F("Aborting ..."));
				reflowPhase = PHASE_ABORT_REFLOW;
				displayAdjustmentsMadeContinue(false);
			}

			return;
		}
		else
		{
			/*
			 * It is bad to make adjustments when not in learning mode,
			 * because this leads to inconsistent results.
			 * However, this situation cannot be ignored.
			 * Increase the duty cycle slightly but don't abort the reflow
			 */
			adjustPhaseDutyCycle(reflowPhase, 1);
			Serial.println(F("Duty cycles increased slightly for future runs"));

			// Turn all the elements on to get to temp quickly
			for (int i = 0; i < 4; ++i )
			{
				if ( outputType[i] == TYPE_TOP_ELEMENT
						|| outputType[i] == TYPE_BOTTOM_ELEMENT
						|| outputType[i] == TYPE_BOOST_ELEMENT )
					phase[reflowPhase].elementDutyCycle[i] = 100;
			}

			// Extend this phase by 5 seconds, or abort the reflow if it has taken too long
			if ( phase[reflowPhase].phaseMaxDuration < 200 )
				phase[reflowPhase].phaseMaxDuration += 5;
			else
			{
				lcdPrintPhaseMessage(reflowPhase, "Too slow");
				lcdPrintLineF(1, F("Aborting ..."));
				reflowPhase = PHASE_ABORT_REFLOW;
				Serial.println(F("Aborting reflow.  Oven cannot reach required temp!"));
			}
		}
	}

	// Turn the output on or off based on its duty cycle
	for ( int i = 0; i < 4; ++i )
	{
		// Skip unused elements and cooling fan
		if ( outputType[i] == TYPE_UNUSED || outputType[i] == TYPE_COOLING_FAN )
			continue;

		// Turn all the elements on at the start of the presoak
		if ( reflowPhase == PHASE_PRESOAK && currentTemp < (phase[reflowPhase].endTemp * 3 / 5) )
		{
			digitalWrite(4 + i, HIGH);
			continue;
		}

		// Turn the output on at 0, and off at the duty cycle value
		if ( elementDutyCounter[i] == 0 )
			digitalWrite(4 + i, HIGH);

		if ( elementDutyCounter[i] == phase[reflowPhase].elementDutyCycle[i] )
			digitalWrite(4 + i, LOW);

		// Increment the duty counter
		elementDutyCounter[i] = (elementDutyCounter[i] + 1) % 100;
	}

	// Don't consider the reflow process started until the temp passes 50 degrees
	if ( currentTemp < 50.0 )
		phaseStartTime = currentTime;

	// Update the displayed temp roughly once per second
	if ( ! (counter++ % 20) )
		displayReflowTemp(currentTime, reflowStartTime, phaseStartTime, currentTemp);
}

void phaseWaiting(const double currentTemp, const unsigned long currentTime)
{
	if ( firstTimeInPhase )
	{
		firstTimeInPhase = false;
		// Update the display
		lcdPrintLineF(0, F("Reflow"));
		lcdPrintLine(1, " ");
		Serial.println(F("******* Phase: Waiting *******"));
		Serial.println(F("Turning all heating elements off ..."));

		// Make sure all the elements are off (keep convection fans on)
		for ( int i = 0; i < 4; ++i )
		{
			if ( outputType[i] != TYPE_CONVECTION_FAN )
			digitalWrite(i+4, LOW);
		}

		// If we made it here it means the reflow is within the defined parameters.  Turn off learning mode
		Settings::set(Settings::LEARNING_MODE, false);
	}

	// Update the displayed temp roughly once per second
	if ( ! (counter++ % 20) )
	{
		displayReflowTemp(currentTime, reflowStartTime, phaseStartTime, currentTemp);
		// Countdown to the end of this phase
		lcd.setCursor(13, 0);
		lcd.print(40 - ((currentTime - phaseStartTime) / MILLIS_TO_SECONDS));
		lcd.print("s ");
	}

	// Wait in this phase for 40 seconds.  The maximum time in liquidous state is 150 seconds
	// Max 90 seconds in PHASE_REFLOW + 40 seconds in PHASE_WAITING + some cool down time in PHASE_COOLING_BOARDS_IN is less than 150 seconds.
	if ( currentTime - phaseStartTime > 40 * MILLIS_TO_SECONDS )
	{
		reflowPhase = PHASE_COOLING_BOARDS_IN;
		firstTimeInPhase = true;
	}
}

void phaseCoolingBoardsIn(const double currentTemp, const unsigned long currentTime)
{
	if ( firstTimeInPhase )
	{
		firstTimeInPhase = false;
		// Update the display
		lcdPrintLineF(0, F("Cool - open door"));
		Serial.println(F("******* Phase: Cooling *******"));
		Serial.println(F("Open the oven door ..."));
		// If a servo is attached, use it to open the door over 10 seconds
		setServoPosition(Settings::get(Settings::SERVO_OPEN_DEGREES), 10000);
		// Play a tune to let the user know the door should be opened
		Tunes::playReflowComplete();

		// Turn on the cooling fan
		for ( int i = 0; i < 4; ++i )
		{
			if ( outputType[i] == TYPE_COOLING_FAN )
			digitalWrite(i+4, HIGH);
		}
	}

	// Update the temp roughly once per second
	if ( ! (counter++ % 20) )
		displayReflowTemp(currentTime, reflowStartTime, phaseStartTime, currentTemp);

	// Boards can be removed once the temp drops below 100C
	if ( currentTemp < 100.0 )
	{
		reflowPhase = PHASE_COOLING_BOARDS_OUT;
		firstTimeInPhase = true;
	}
}

void phaseCoolingBoardsOut(const double currentTemp, const unsigned long currentTime)
{
	if ( firstTimeInPhase )
	{
		firstTimeInPhase = false;
		// Update the display
		lcdPrintLineF(0, F("Okay to remove  "));
		lcdPrintLineF(1, F("boards"), 10);
		// Play a tune to let the user know the boards can be removed
		Tunes::playRemoveBoards();
	}

	// Update the temp roughly once per second
	if ( ! (counter++ % 20) )
		displayReflowTemp(currentTime, reflowStartTime, phaseStartTime, currentTemp);

	// Once the temp drops below 50C a new reflow can be started
	if ( currentTemp < 50.0 )
	{
		reflowPhase = PHASE_ABORT_REFLOW;
		lcdPrintLineF(0, F("Reflow complete!"));
		lcdPrintLine(1, " ");
	}
}

void phaseAbortReflow(void)
{
	Serial.println(F("Reflow is done!"));
	// Turn all elements and fans off
	for ( int i = 4; i < 8; ++i )
		digitalWrite(i, LOW);

	// Close the oven door now, over 3 seconds
	setServoPosition(Settings::get(Settings::SERVO_CLOSED_DEGREES), 3000);
	// Start next time with initialization
	reflowPhase = PHASE_INIT;

	// Wait for a bit to allow the user to read the last message
	delay(3000);
}

// For debugging you can use this function to simulate the thermocouple
/*double getTemp(int bakeDutyCycle) {
  static double counterTemp = 29.0;
  static int phase = 0;
  double diff[] = {0.08, 0.03, 0.05, -0.26};
  double tempChange[] = {140, 215, 255, 40};
  static long counter = 0;
  static double offset = 0.001;

  static double dir = 0.19;

  if (bakeDutyCycle == 100)
    offset = 0.001;
  if ((++counter % 20000) == 0) {
    Serial.print("Offset was ");
    Serial.print(int (offset * 1000));
    Serial.print("  Duty = ");
    Serial.println(bakeDutyCycle);
    offset += 0.0005;
  }

  if (counterTemp < 99 && counterTemp > 96)
    dir = (0.0003 * bakeDutyCycle) - offset;
  if (counterTemp > 100.5)
    dir = -0.013;
  counterTemp += dir;
  return counterTemp;

  if (diff[phase] > 0 && counterTemp > tempChange[phase])
    phase++;
  else if (diff[phase] < 0 && counterTemp < tempChange[phase])
    phase++;
  phase = phase % 4;
  counterTemp += diff[phase];
}*/

} // namespace

bool Reflow(void)
{
	const unsigned long currentTime(millis());
	double currentTemp(0.0);
	int fault(getCurrentTemp(currentTemp));

	if ( fault )
		thermocoupleFault(fault);

	if ( getButton() != CONTROLEO_BUTTON_NONE )
		abortReflow();

	int elementDutyStart(0);

	switch ( reflowPhase )
	{
	case PHASE_INIT: // User has requested to start a reflow
		phaseInit(elementDutyStart, currentTemp);
		break;

	case PHASE_PRESOAK:
	case PHASE_SOAK:
	case PHASE_REFLOW:
		phaseHeat(elementDutyStart, currentTemp, currentTime);
		break;

	case PHASE_WAITING: // Wait for solder to reach max temps and start cooling
		phaseWaiting(currentTemp, currentTime);
		break;

	case PHASE_COOLING_BOARDS_IN: // Start cooling the oven.  The boards must remain in the oven to cool
		phaseCoolingBoardsIn(currentTemp, currentTime);
		break;

	case PHASE_COOLING_BOARDS_OUT: // The boards can be removed without dislodging components now
		phaseCoolingBoardsOut(currentTemp, currentTime);
		break;

	case PHASE_ABORT_REFLOW: // The reflow must be stopped now
		phaseAbortReflow();
		return false; // Return to the main menu
	}

	return true;
}

