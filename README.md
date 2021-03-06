****************************
ReflowWizard2Plastic
Enhanced Reflow Wizard
supports drying/reconditioning PLA, ABS, PETG, Nylon and Desiccant
Youtube video:
    https://www.youtube.com/watch?v=zqyhKac-VSQ&feature=youtu.be
****************************

2016.10.18

This version is currently based on the Controleo2 Reflow Wizard version 1.9
It has been cleaned up and reorganized to match my C++ sensibilities
Working ram has been dramatically improved due to much more aggresive use of PROGMEM

Install the library/Controleo2 directory into your Arduino/libraries directory

A Makefile has been provided for those of us using Linux to simplify installation on the reflow oven
use "make" to test the build
use "make upload" to install on the reflow oven (of course the oven must be connected by usb)

You can also build/install using the Arduino Ide as per usual, you want to select leonardo as the board type

ControLeo2 Reflow Oven Controller
=================================

Reflow oven build guide:
http://www.whizoo.com/reflowoven

Buying ControLeo2:
http://www.whizoo.com/buy

Updating the firmware running on ControLeo2:
http://www.whizoo.com/update

This is the GitHub source code repository for ControLeo2.  ControLeo2 is an Arduino Leonardo controller with quad relay outputs used to control reflow ovens.

In this folder are:
1. README - this file
2. A "ControLeo2” folder.  This is an Arduino library and examples for ControLeo2

To install the ControLeo2 library, please refer to:
http://arduino.cc/en/Guide/Libraries or http://www.whizoo.com/update

Reflow Wizard
=============
* 1.0  Initial public release. (21 October 2014)
* 1.1  Bug fixes (30 December 2014)
*      - Oven temperature might not reach configured maximum temperature
*      - Adjusted values so learning happens faster
*      - Other minor improvements
* 1.2  Improvements (6 January 2014)
*      - Take temperature readings every 0.125s and average them.  Fixes errors
*        generated by noise from convection fans
*      - Refined learning mode so learning should happen in fewer runs
*      - Improved loop duration timer to enhance timing
*      - Moved some strings from RAM to Flash (there is only 2.5Kb of RAM)
* 1.3  No user-facing changes (19 January 2013)
*      - Fixed compiler warnings for Arduino 1.5.8
* 1.4  Added support for servos (6 February 2015)
*      - When using a servo, please connect a large capacitor (e.g. 220uF, 16V)
*        between +5V and GND to avoid a microcontroller reset when the servo
*        stall current causes the voltage to drop.  Small servos may not need
*        a capacitor
*      - Connect the servo to +5V, GND and D3
*      - The open and close positions are configured in the Settings menu
*      - The oven door will open when the reflow is done, and close at 50C.
*        The door is also closed when ControLeo2 is turned on.
* 1.5 Minor improvements (15 July 2015)
*      - Made code easier to read by using “F” macro for strings stored in flash
*      - Minor adjustments to reflow values 
*      - Restrict the maximum duty cycle for the boost element to 60%.  It
*        should never need more than this!
*      - Make thermocouple more tolerant of transient errors, including
*        FAULT_OPEN
* 1.6 Added ability to bake (29 March 2016)
*      - Ability to bake was requested by a number of users, and was implemented
*        by Bernhard Kraft and Mark Foster.  Bernhard's implementation has been
*        added to the code base.  This is a simple algorithm that only implements
*        the ”P” (proportional) of the PID algorithm.  Temperatures may overshoot
*        initially, but for the rest of the baking time (up to 18 hours) the
*        temperature will be a few degrees below the target temperature.
*      - Added option to Setup menu to restart learning mode.
*      - Added option to Setup menu to reset to factory mode (erase everything) 
* 1.7 Added support for cooling fan (7 June 2106)
*      - Any of the 4 outputs can be configured to control a cooling fan
*      - When bake finishes, the servo will open the oven door (thanks jcwren)
* 1.8 Major changes to bake functionality (22 June 2016)
*      - Complete rewrite of bake functionality.  Users reported that ControLeo2
*        would sometimes freeze during bake, leaving one or more elements on.
*      - Added Integral (the "I" of PID) to bake, so temperatures come closer
*        to desired bake temperature
*      - Added bake countdown timer to LCD screen
*      - Fixed bug where oven door would not close once bake completed.
* 1.9  Minor change to bake (25 June 2016)
*      - Initial bake duty cycle is based on desired bake temperature


Oven door servo:
================
Version 1.4 adds support for a servo to open the oven door when the reflow is done.  The door should be opened 1” - 2” so the temperature drop does not exceed 6°C/second.  Small servos (like the TG9) are probably powerful enough to open the oven door. Large servos (like the MG945 with metal gears) will work too - with a caveat.  Large servos mean large stall currents which can cause the voltage to the microcontroller to drop and cause a brown-out reset.  To prevent this from happening, it is recommended that you place a large capacitor (e.g. 220uF, 16V) between +5V and GND.

The power supply used to power ControLeo2 should provide at least 1000mA, but preferably 1.5A or even 2A.  The power supplies provided in the ControLeo2 oven build kit are 2A. 


Peter Easton 2016
whizoo.com


