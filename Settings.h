#ifndef _SETTINGS_h
#define _SETTINGS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#define LED 13
#define CLOCKPIN 14		// incoming voltage clock
#define TEMPOPIN 9		// analog pin 9 A9 - pot for controlling clock manually
#define ENCCLKPIN 5
#define ENCDATAPIN 6
#define GATEOUT 16		// Gate sequence out
#define DACPIN 40		// CV sequence out

#define OLED_MOSI  11
#define OLED_CLK   20
#define OLED_DC    8
#define OLED_CS    10
#define OLED_RESET 9

// edit modes: STEPV voltage; STEPR random level; STUTTER stutter count, PATTERN pattern number, STEPS in pattern, RANDALL - randomise all settings, RANDVALS - randomise just values
enum editType { STEPV, STEPR, STUTTER, PATTERN, STEPS, LOOPFIRST, LOOPLAST, SEQOPT, RANDALL, RANDVALS, SETUP };
 
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
enum btnName { STEPUP, STEPDN, ENCODER, CHANNEL, ACTION, ENCUP, ENCDN };

struct MenuItem {
	int val;
	String name;
	boolean selected;
};


enum menuItems { EXIT, SAVE };

#endif

