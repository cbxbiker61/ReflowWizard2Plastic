// Bake logic
// Called from the main loop 20 times per second
// This where the bake logic is controlled

#include <Arduino.h>
#include "ReflowWizard.h"

namespace {

#define MILLIS_TO_SECONDS ((long) 1000)

#define PHASE_INIT          0 // Initialize baking, check oven temp
#define PHASE_HEATUP        1 // Heat up the oven rapidly to just under the desired temp
#define PHASE_BAKE          2 // The main baking phase. Just keep the oven temp constant
#define PHASE_START_COOLING 3 // Start the cooling process
#define PHASE_COOLING       4 // Wait till the oven has cooled down to 50°C
#define PHASE_ABORT         5 // Baking was aborted or completed

const char NULL_FSTR[] PROGMEM = "";
const char HEATING_FSTR[] PROGMEM = "Heating";
const char BAKING_FSTR[] PROGMEM = "Baking";
const char REFRESHING_PLA_FSTR[] PROGMEM = "Refreshing PLA";
const char DRYING_PLA_FSTR[] PROGMEM = "Drying PLA";
const char DRYING_ABS_FSTR[] PROGMEM = "Drying ABS";
const char DRYING_PETG_FSTR[] PROGMEM = "Drying PETG";
const char DRYING_NYLON_FSTR[] PROGMEM = "Drying Nylon";
const char DRYING_DESICCANT_FSTR[] PROGMEM = "Drying Desiccant";
const char COOLING_FSTR[] PROGMEM = "Cooling";
const char *phaseDesc[] = { NULL_FSTR, HEATING_FSTR, BAKING_FSTR, NULL_FSTR, COOLING_FSTR, NULL_FSTR };

int currentPhase(PHASE_INIT);
int outputType[4];
int bakeTemp;
uint32_t bakeDuration;
bool doorOpen;
bool parmsSet;
int elementDutyCounter[4];
int bakeDutyCycle;
int bakeIntegral;
int counter;
int coolingDuration;
bool isHeating;
long lastOverTempTime;

// Display the current temp to the LCD screen and print it to the serial port so it can be plotted
void displayBakeTime(uint32_t duration, const double temp, int duty, int integral)
{
	displayTemp(temp);

	char buf[100];
	// Write the time and temp to the serial port, for graphing or analysis on a PC
	snprintf(buf, sizeof(buf), "%lu, %i, %i, ", duration, duty, integral);
	Serial.print(buf);
	Serial.println(temp);

	displayDuration(10, duration);
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

	// Abort the bake
	Serial.println(F("Bake aborted because of thermocouple error!"));
	currentPhase = PHASE_ABORT;
	delay(3000);
}

void abortBake(void)
{
	currentPhase = PHASE_ABORT;
	lcdPrintLineF(0, F("Aborting bake"));
	lcdPrintLineF(1, F("Button pressed"));
	Serial.println(F("Button pressed.  Aborting bake ..."));
	delay(2000);
}

void phaseInit(void)
{
	// Start the bake, regardless of the starting temp
	// Get the types for the outputs (elements, fan or unused)
	for ( int i = 0; i < 4; ++i )
		outputType[i] = Settings::get(Settings::D4_TYPE + i);

	Serial.print(F("Baking temp = "));
	Serial.println(bakeTemp);
	Serial.print(F("Baking duration = "));
	Serial.println(bakeDuration);

	for ( int i = 0; i < 4; ++i )
	{
		if ( outputType[i] == TYPE_CONVECTION_FAN )
			digitalWrite(4 + i, HIGH);
	}

	// Move to the next phase
	currentPhase = PHASE_HEATUP;
	lcdPrintLineF(0, (const __FlashStringHelper *)phaseDesc[currentPhase]);
	lcdPrintLine(1, "");

	// Start with a duty cycle proportional to the desired temp
	bakeDutyCycle = map(bakeTemp, 0, 250, 0, 100);

	isHeating = true;
	bakeIntegral = 0;
	counter = 0;

	// Stagger the element start cycle to avoid abrupt changes in current draw
	// Simple method: there are 4 outputs so space them apart equally
	for ( int i = 0; i< 4; ++i )
		elementDutyCounter[i] = 25 * i;
}

void phaseHeatup(bool displayBake, const double currentTemp)
{
	if ( displayBake )
	{
		// Display the remaining time
		displayBakeTime(bakeDuration, currentTemp, bakeDutyCycle, bakeIntegral);

		// Don't start decrementing bakeDuration until close to baking temp
	}

	// Is the oven close to the desired temp?
	if ( bakeTemp - currentTemp < 15.0 )
	{
		currentPhase = PHASE_BAKE;
		lcdPrintLineF(0, (const __FlashStringHelper *)phaseDesc[currentPhase]);
		// Reduce the duty cycle for the last 10 degrees
		bakeDutyCycle = bakeDutyCycle / 3;
		Serial.println(F("Move to bake phase"));
	}
}

void phaseBake(const double currentTemp)
{
	displayBakeTime(bakeDuration, currentTemp, bakeDutyCycle, bakeIntegral);

	if ( ! (--bakeDuration) ) // Has the bake duration been reached?
	{
		currentPhase = PHASE_START_COOLING;
		return;
	}

	// Is the oven too hot?
	if ( currentTemp > bakeTemp )
	{
		if ( isHeating )
		{
			isHeating = false;

			// Turn all heating elements off
			for ( int i = 0; i < 4; ++i )
			{
				if ( isHeatingElement(outputType[i]) )
					digitalWrite(4 + i, LOW);
			}

			// The duty cycle caused the temp to exceed the bake temp, so decrease it
			// (but not more than once every 30 seconds)
			if ( millis() - lastOverTempTime > (30 * MILLIS_TO_SECONDS) )
			{
				lastOverTempTime = millis();

				if ( bakeDutyCycle > 0 )
					--bakeDutyCycle;
			}

			// Reset the bake integral, so it will be slow to increase the duty cycle again
			bakeIntegral = 0;
			Serial.println(F("Over-temp. Elements off"));
		}

		return;
	}

	isHeating = true;

	// Increase the bake integral if not close to temp
	if ( bakeTemp - currentTemp > 1.0 )
		++bakeIntegral;

	// Has the oven been under-temp for a while?
	if ( bakeIntegral > 30 )
	{
		bakeIntegral = 0;

		if ( bakeDutyCycle < 100 )
			++bakeDutyCycle;

		Serial.println(F("Under-temp. Increasing duty cycle"));
	}
}

void phaseStartCooling(void)
{
	Serial.println(F("Starting cooling"));
	isHeating = false;

	// Turn off all elements and turn on the fans
	for ( int i = 0; i < 4; ++i )
	{
		switch ( outputType[i] )
		{
		case TYPE_CONVECTION_FAN:
		case TYPE_COOLING_FAN:
			digitalWrite(4 + i, HIGH);
			break;

		default:
			digitalWrite(4 + i, LOW);
			break;
		}
	}

	// Move to the next phase
	currentPhase = PHASE_COOLING;
	lcdPrintLineF(0, (const __FlashStringHelper*)phaseDesc[currentPhase]);

	// If a servo is attached, use it to open the door over 10 seconds
	setServoPosition(Settings::get(Settings::SERVO_OPEN_DEGREES), 10000);
	Tunes::playReflowComplete();

	// Cooling should be at least 1 minute (60 seconds) in duration
	coolingDuration = 60;
}

void phaseCooling(const double currentTemp)
{
	// Display the remaining time
	displayBakeTime(bakeDuration, currentTemp, bakeDutyCycle, bakeIntegral);

	if ( coolingDuration > 0 ) // Wait in this phase until the oven has cooled
		--coolingDuration;

	const double finishTemp(doorOpen ? 30.0 : 50.0);

	if ( currentTemp < finishTemp && coolingDuration == 0 )
		currentPhase = PHASE_ABORT;
}

void phaseAbort(void)
{
	Serial.println(F("Bake is done!"));
	isHeating = false;

	// Turn all elements and fans off
	for ( int i = 4; i < 8; ++i )
		digitalWrite(i, LOW);

	// Close the oven door now, over 3 seconds
	setServoPosition(Settings::get(Settings::SERVO_CLOSED_DEGREES), 3000);
	// Start next time with initialization
	currentPhase = PHASE_INIT;

	parmsSet = false;
}

void manageHeating(void)
{
	for ( int i = 0; i < 4; ++i )
	{
		switch ( outputType[i] )
		{
		case TYPE_TOP_ELEMENT:
		case TYPE_BOTTOM_ELEMENT:
			// Turn the output on at 0, and off at the duty cycle value
			if ( elementDutyCounter[i] == 0 )
				digitalWrite(4 + i, HIGH);

			if ( elementDutyCounter[i] == bakeDutyCycle )
				digitalWrite(4 + i, LOW);

			break;

		case TYPE_BOOST_ELEMENT: // Give it half the duty cycle of the other elements
			// Turn the output on at 0, and off at the duty cycle value
			if ( elementDutyCounter[i] == 0 )
				digitalWrite(4 + i, HIGH);

			if ( elementDutyCounter[i] == bakeDutyCycle / 2 )
				digitalWrite(4 + i, LOW);

			break;

		default:
			break; // Skip unused elements and fans
		}

		// Increment the duty counter
		elementDutyCounter[i] = (elementDutyCounter[i] + 1) % 100;
	}
}

// Return false to exit this mode
bool localBake(void)
{
	bool isOneSecondInterval(false);

	// Determine if this is on a 1-second interval
	if ( ++counter >= 20 )
	{
		counter = 0;
		isOneSecondInterval = true;
	}

	double currentTemp(0.0);
	int fault(getCurrentTemp(currentTemp));

	if ( fault )
		thermocoupleFault(fault);

	if ( getButton() != CONTROLEO_BUTTON_NONE )
		abortBake();

	switch ( currentPhase )
	{
	case PHASE_INIT: // User has requested to start a bake
		phaseInit();
		break;

	case PHASE_HEATUP:
		phaseHeatup(isOneSecondInterval, currentTemp);
		break;

	case PHASE_BAKE:
		if ( isOneSecondInterval )
			phaseBake(currentTemp);
		break;

	case PHASE_START_COOLING:
		phaseStartCooling();
		break;

	case PHASE_COOLING:
		if ( isOneSecondInterval )
			phaseCooling(currentTemp);
		break;

	case PHASE_ABORT:
		phaseAbort();
		return false; // Return to the main menu
	}

	if ( isHeating )
		manageHeating();

	return true;
}

void initParms(int temp, uint32_t duration, bool _doorOpen, const char *bakeDesc)
{
	bakeTemp = temp;
	bakeDuration = duration;
	doorOpen = _doorOpen;
	phaseDesc[PHASE_BAKE] = bakeDesc;

	if ( doorOpen )
	{
		unsigned int op(Settings::get(Settings::SERVO_OPEN_DEGREES));
		unsigned int cl(Settings::get(Settings::SERVO_CLOSED_DEGREES));
		unsigned int part(cl);

		if ( op > cl )
			part = cl + ((op - cl) / 3);
		else
			part = cl - ((cl - op) / 3);

		setServoPosition(part, 2000);
	}

	parmsSet = true;
}

} // namespace

bool Bake(void)
{
	if ( ! parmsSet )
		initParms(Settings::get(Settings::BAKE_TEMP)
				, getBakeSeconds(Settings::get(Settings::BAKE_DURATION))
				, false, BAKING_FSTR);

	return localBake();
}

#define HOURS_1 (60UL*60UL)
#define HOURS_2 (HOURS_1*2UL)
#define HOURS_3 (HOURS_1*3UL)
#define HOURS_4 (HOURS_1*4UL)
#define HOURS_5 (HOURS_1*5UL)
#define HOURS_13 (HOURS_1*13UL)

bool RefreshPla(void)
{
	if ( ! parmsSet )
		initParms(63, HOURS_1, true, REFRESHING_PLA_FSTR);

	return localBake();
}

bool DryPla(void)
{
	if ( ! parmsSet )
		initParms(45, HOURS_5, true, DRYING_PLA_FSTR);

	return localBake();
}

bool DryAbs(void)
{
	if ( ! parmsSet )
		initParms(62, HOURS_3, true, DRYING_ABS_FSTR);

	return localBake();
}

bool DryPetg(void)
{
	if ( ! parmsSet )
		initParms(65, HOURS_3, true, DRYING_PETG_FSTR);

	return localBake();
}

bool DryNylon(void)
{
	if ( ! parmsSet )
		initParms(70, HOURS_13, true, DRYING_NYLON_FSTR);

	return localBake();
}

bool DryDesiccant(void)
{
	if ( ! parmsSet )
		initParms(65, HOURS_4, true, DRYING_DESICCANT_FSTR);

	return localBake();
}

// Returns the bake duration, in seconds (max 65536 = 18 hours)
uint32_t getBakeSeconds(int duration)
{
	if ( duration < BAKE_MAX_DURATION )
	{
		uint32_t minutes;

		if ( duration <= 55 ) // 5 to 60 minutes, at 1 minute increments
			minutes = duration + 5;
		else if ( duration <= 91 ) // 1 hour to 4 hours at 5 minute increments
			minutes = (duration - 55) * 5 + 60;
		else // 4+ hours in 10 minute increments
			minutes = (duration - 91) * 10 + 240;

		return minutes * 60;
	}

	return 5;
}

