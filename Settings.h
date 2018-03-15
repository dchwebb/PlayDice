// Settings.h

#ifndef _SETTINGS_h
#define _SETTINGS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#define LED 13
#define CLOCKPIN 0
#define TEMPOPIN 9
#define DACPIN 40
#define BTNUPPIN 12
#define BTNDNPIN 11
#define BTNENCPIN 10
#define ENCCLKPIN 5
#define ENCDATAPIN 6
#define OLED_RESET 4

enum refreshType { REFRESHOFF, REFRESHFULL, REFRESHTOP, REFRESHBOTTOM };
enum editType { STEPV, STEPR, STUTTER, SEQS, SEQMODE, PATTERN };		// STEPV allows deep editing of voltage; STEPR - editing random level; STUTTER - choose stutter speed, PATTERN - choose pattern

// define structures to store sequence data
struct Step {
	float volts;
	unsigned int rand_amt : 4; // from 0 to 10
	unsigned int subdiv : 5;
};
struct Sequence {
	int type : 4;		//  Sample and Hold, gated etc
	unsigned int mode : 4;		//	CURRENT - Loop current sequence, ALL - run through sequences
	unsigned int steps : 4;		//  Number of steps in sequence
	struct Step Steps[8];
};
struct Patterns {
	struct Sequence seq[8];
};
enum seqType { CV, GATE, PITCH };
enum seqMode { LOOPCURRENT, LOOPALL };
enum seqInitType { INITRAND, INITBLANK };


//	state handling for momentary buttons
struct Btn {
	int name;
	int pin;
	boolean pressed;
	int lastPressed;
};
enum btnName { STEPUP, STEPDN, ENCODER };

/*
class Settings {
public:
	enum refreshType2 { REFRESHOFF, REFRESHFULL, REFRESHTOP, REFRESHBOTTOM };


};
*/
#endif

