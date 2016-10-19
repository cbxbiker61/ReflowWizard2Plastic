// Thermocouple
// Instead of getting instantaneous readings from the thermocouple, get an average
// Also, some convection ovens have noisy fans that generate spurious short-to-ground and
// short-to-vcc errors.  This will help to eliminate those.
// takeCurrentThermocoupleReading() is called from the Timer 1 interrupt (see "Servo" tab).  It is
// called 5 times per second.

#include <Arduino.h>
#include <ControLeo2.h>
#include "ReflowWizard.h"

#define NUM_READINGS     5   // Number of readings to average the temp over (5 readings = 1 second)
#define ERROR_THRESHOLD 15  // Number of consecutive faults before a fault is returned

// Store the temps as they are read
volatile double recentTemps[NUM_READINGS];
volatile int tempFaultCount;
volatile int tempFault;
ControLeo2::MAX31855 thermocouple;

// This function is called every 200ms from the Timer 1 (servo) interrupt
void takeCurrentThermocoupleReading(void)
{
	volatile static int readingNum;

	// The timer has fired.  It has been 0.2 seconds since the previous reading was taken
	// Take a thermocouple reading
	double temp(9999.9);

	if ( thermocouple.readThermocouple(temp) )
	{
		recentTemps[readingNum] = temp;
		readingNum = (readingNum + 1) % NUM_READINGS;
		// Clear any previous error
		tempFaultCount = 0;
	}
	else // error
	{
		/*
		 * Noise can cause spurious short faults.
		 * These are typically caused by the convection fan
		 */
		if ( tempFaultCount < ERROR_THRESHOLD )
			++tempFaultCount;

		tempFault = thermocouple.getFault();
	}
}

// Routine used by the main app to get temps
// This routine disables and then re-enables interrupts so that data corruption isn't caused
// by the ISR writing data at the same time it is read here.
int getCurrentTemp(double &target)
{
	target = 0.0;
	int rc(0);

	noInterrupts();

	if ( tempFaultCount < ERROR_THRESHOLD )
	{
		for ( int i = 0; i < NUM_READINGS; ++i )
			target += recentTemps[i];

		target /= NUM_READINGS;
	}
	else
	{
		target = 9999.9;
		rc = tempFault;
	}

	interrupts();

	return rc;
}

