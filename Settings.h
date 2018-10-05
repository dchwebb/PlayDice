#ifndef _SETTINGS_h
#define _SETTINGS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#define DEBUGSTEP 0
#define DEBUGCLOCK 0
#define DEBUGRAND 0
#define DEBUGQUANT 1
#define DEBUGBTNS 0

#define LED 13
#define CLOCKPIN 14		// incoming voltage clock
#define TEMPOPIN 9		// analog pin 9 - pot for controlling clock manually
#define ENCCLKPIN 16	// encoder pin 1
#define ENCDATAPIN 17	// encoder pin 3
#define GATEOUT 18		// Gate sequence out
#define DACPIN 40		// CV sequence out


#define OLED_CS    9
#define OLED_DC    8
#define OLED_RESET 7
#define OLED_MOSI  6		// D1 on OLED
#define OLED_CLK   5		// D0 on OLED

// edit modes: STEPV voltage; STEPR random level; STUTTER stutter count, PATTERN pattern number, STEPS in pattern, RANDALL - randomise all settings, RANDVALS - randomise just values
enum editType { STEPV, STEPR, STUTTER, PATTERN, STEPS, LOOPFIRST, LOOPLAST, SEQOPT, RANDALL, RANDVALS, SETUP, SUBMENU, LFO, NOISE};

// action mode - what happens when the action button is pressed
enum actionOpts { ACTSTUTTER, ACTRESTART, ACTPAUSE };

// define structures to store sequence data
struct CvStep {
	float volts;
	uint8_t rand_amt : 4; // from 0 to 10
	uint16_t stutter : 5;
};
struct GateStep {
	uint16_t on : 1;
	uint8_t rand_amt : 4; // from 0 to 10
	uint16_t stutter : 5;
};
struct CvSequence {
	uint8_t steps : 4;		//  Number of steps in sequence
	struct CvStep Steps[8];
};
struct GateSequence {
	uint8_t steps : 4;		//  Number of steps in sequence
	struct GateStep Steps[8];
};

struct CvPatterns {
	struct CvSequence seq[8];
};
struct GatePatterns {
	struct GateSequence seq[8];
};
enum seqMode { LOOPCURRENT, LOOPALL };
enum seqInitType { INITRAND, INITVALS, INITBLANK};
enum seqType { SEQCV, SEQGATE };
enum rndType { UPPER, LOWER };

//	state handling for momentary buttons
struct Btn {
	int name;
	int pin;
	boolean pressed;
	boolean released;
	boolean longClick;
	uint32_t lastPressed;
};
enum btnName { STEPUP, STEPDN, ENCODER, CHANNEL, ACTIONBTN, ENCUP, ENCDN, ACTIONCV };

struct MenuItem {
	int pos;
	String name;
	boolean selected;
	String val = "";
};

struct QuantiseRange {
	float to;		// the upper range of voltage included in this quantise step
	float target;	// the voltage to shift the CV to if in this step
};
#endif

