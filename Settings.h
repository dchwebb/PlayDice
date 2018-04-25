// Settings.h

#ifndef _SETTINGS_h
#define _SETTINGS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#define LED 13
//#define CLOCKPIN 0
#define CLOCKPIN 14
#define TEMPOPIN 9		// analog pin 9 A9
#define DACPIN 40
#define BTNACTION 9		// digital pin 9
#define BTNUPPIN 1
#define BTNDNPIN 0
#define BTNENCPIN 2
#define ENCCLKPIN 5
#define ENCDATAPIN 6
#define OLED_RESET 4
#define GATEOUT 16

enum refreshType { REFRESHOFF, REFRESHFULL, REFRESHTOP, REFRESHBOTTOM };
enum editType { STEPV, STEPR, STUTTER, SEQS, SEQMODE, PATTERN };		// STEPV allows deep editing of voltage; STEPR - editing random level; STUTTER - choose stutter speed, PATTERN - choose pattern

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
	uint8_t mode : 4;		//	CURRENT - Loop current sequence, ALL - run through sequences
	uint8_t steps : 4;		//  Number of steps in sequence
	struct CvStep Steps[8];
};
struct GateSequence {
	uint8_t mode : 4;		//	CURRENT - Loop current sequence, ALL - run through sequences
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
enum seqInitType { INITRAND, INITBLANK };
enum seqType { SEQCV, SEQGATE };
enum rndType { UPPER, LOWER };

//	state handling for momentary buttons
struct Btn {
	int name;
	int pin;
	boolean pressed;
	int lastPressed;
};
enum btnName { STEPUP, STEPDN, ENCODER, ACTION };

/*
class Settings {
public:
	enum refreshType2 { REFRESHOFF, REFRESHFULL, REFRESHTOP, REFRESHBOTTOM };


};
*/
#endif

