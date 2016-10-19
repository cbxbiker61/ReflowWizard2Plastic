#pragma once
// Written by Peter Easton
// Released under WTFPL license
//
// Change History:
// 14 August 2014        Initial Version

#include <Arduino.h>

namespace ControLeo2 {

class MAX31855
{
public:
	MAX31855();

#if 0
	enum {
		FAULT_OPEN = 10000
		, FAULT_SHORT_GND = 10001
		, FAULT_SHORT_VC = 10002
	};
#endif

	/* returns false for thermocouple fault */
	bool readThermocouple(double &target, bool wantFahrenheit = false);
	bool isFaultOpen(void) const { return _fault == 1; }
	bool isFaultShortGnd(void) const { return _fault == 2; }
	bool isFaultShortVcc(void) const { return _fault == 3; }
	int getFault(void) { return _fault; }
	void readJunction(double &target, bool wantFahrenheit = false);

private:
	uint32_t getRawData(void);

	int _fault;
};

} // namespace ControLeo2

