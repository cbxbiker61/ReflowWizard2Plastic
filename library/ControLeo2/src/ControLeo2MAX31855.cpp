#include "ControLeo2MAX31855.h"

namespace {

const int MISO_PIN(8);
const int CS_PIN(9);
const int CLK_PIN(10);

} // namespace

namespace ControLeo2 {

MAX31855::MAX31855()
	: _fault(0)
{
	// MAX31855 data output pin
	pinMode(MISO_PIN, INPUT);
	// MAX31855 chip select input pin
	pinMode(CS_PIN, OUTPUT);
	// MAX31855 clock input pin
	pinMode(CLK_PIN, OUTPUT);

	// Default output pins state
	digitalWrite(CS_PIN, HIGH);
	digitalWrite(CLK_PIN, LOW);
}

/*
 * Read the thermocouple temp.
 * Internally, the conversion takes place in the background within 100 ms.
 * Values are updated only when the CS line is high.
 *
 * Return	    Description
 * =========		===========
 * temp	Temperature of the thermocouple either in Degree Celsius or
 * Fahrenheit. If fault is detected, FAULT_OPEN, FAULT_SHORT_GND or
 * FAULT_SHORT_VCC will be returned. These fault values are outside
 * of the temperature range the MAX31855 is capable of.
 */
bool MAX31855::readThermocouple(double &target, bool wantFahrenheit)
{
	_fault = 0;
	target = 9999.9;

	uint32_t data(getRawData());
	//double temp(0.0);

	if ( ! (data & 0x00010000) ) // no fault detected
	{
		// Retrieve thermocouple temp data and strip redundant data
		data = data >> 18;
		// Bit-14 is the sign
		target = (data & 0x00001FFF);

		// Check for negative temp
		if ( data & 0x00002000 )
		{
			// 2's complement operation
			// Invert
			data = ~data;
			// Ensure operation involves lower 13-bit only
			target = data & 0x00001FFF;
			// Add 1 to obtain the positive number
			target += 1;
			// Make temp negative
			target *= -1;
		}

		// Convert to Degree Celsius
		target *= 0.25;

		if ( wantFahrenheit )
		{
			// Convert Degree Celsius to Fahrenheit
			target = (target * 9.0/5.0)+ 32;
		}

		return true;
	}
	// Check for fault type (3 LSB)
	switch ( data & 0x00000007 )
	{
	case 0x01: _fault = 1; break; // Open circuit
	case 0x02: _fault = 2; break; // Thermocouple short to GND
	case 0x04: _fault = 3; break; // Thermocouple short to VCC
	}

	return false;
}

/*
 * Read the thermocouple temperature in Celsius or Farenheit.
 * Internally, the conversion takes place in the background within 100 ms.
 * Values are updated only when the CS line is high.
 *
 */
void MAX31855::readJunction(double &target, bool wantFahrenheit)
{
	target = 9999.9;
	uint32_t data(getRawData() >> 4); //get raw data and strip fault data bits & reserved bit
	target = data & 0x000007FF; // Bit-12 is the sign

	// Check for negative temp
	if ( data & 0x00000800 )
	{
		// 2's complement operation
		// Invert
		data = ~data;
		// Ensure operation involves lower 11-bit only
		target = data & 0x000007FF;
		// Add 1 to obtain the positive number
		target += 1;
		// Make temp negative
		target *= -1;
	}

	target *= 0.0625; // Convert to Celsius

	if ( wantFahrenheit )
	{
		// Convert Degree Celsius to Fahrenheit
		target = (target * 9.0/5.0)+ 32;
	}
}

/*
 * get 32-bit raw data from MAX31855 chip.
 * Minimum clock pulse width is 100 ns.
 * No delay is required in this case.
 */
uint32_t MAX31855::getRawData(void)
{
	uint32_t data(0);

	digitalWrite(CS_PIN, LOW);

	// Shift in 32-bit of data
	for ( int bitCount = 31; bitCount >= 0; --bitCount )
	{
		digitalWrite(CLK_PIN, HIGH);

		if ( digitalRead(MISO_PIN) ) // data bit is high
			data |= ((uint32_t)1 << bitCount);

		digitalWrite(CLK_PIN, LOW);
	}

	digitalWrite(CS_PIN, HIGH);

	return data;
}

} // namespace ControLeo2

