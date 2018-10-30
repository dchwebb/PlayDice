#pragma once
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
#define DEBUGFRAME 0

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

// edit modes: STEPV voltage; STEPR random level; STUTTER stutter count; PATTERN pattern number; STEPS in pattern; SEQOPTS - randomise settings; SETUP - system menu; LFO/NOISE - lfo or white noise mode
enum editType { STEPV, STEPR, STUTTER, PATTERN, SEQMODE, STEPS, LOOPFIRST, LOOPLAST, SEQOPT, SEQROOT, SEQSCALE, SETUP, SUBMENU, LFO, NOISE };

// action mode - what happens when the action button is pressed
enum actionOpts { ACTSTUTTER, ACTRESTART, ACTPAUSE };

enum seqType { SEQCV, SEQGATE };
enum cvMode { CV, PITCH};
enum gateMode { GATE, TRIGGER };
enum rndType { UPPER, LOWER };

static String const OffOnOpts[] = { "Off", "On" };
String const pitches[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
String const scales[] = { "Chromatic", "Major", "Pentatonic", "Harmonic minor", "Melodic minor" };
uint8_t const scaleSize = 5;
String const scalesShort[] = { "", "", "p", "h", "m" };
String const actions[] = { "Stutter", "Restart", "Pause" };

enum seqInitType { INITNONE, INITRAND, INITVALS, INITBLANK, INITHIGH, INITMEDIUM, INITLOW };
String const initCVSeq[] = { "None", "All", "Vals", "Blank", "High", "Med", "Low" };
uint8_t const initCVSeqSize = 7;
String const initGateSeq[] = { "None", "All", "Vals", "Blank" };
uint8_t const initGateSeqSize = 4;

static boolean scaleNotes[5][12] = { { 1,1,1,1,1,1,1,1,1,1,1,1 },{ 1,0,1,0,1,1,0,1,0,1,0,1 },{ 1,0,0,1,0,1,0,1,0,0,1,0 },{ 1,0,1,1,0,1,0,1,1,0,0,1 },{ 1,0,1,1,0,1,0,1,0,1,0,1 } };

// adds or subtracts one from a number, looping back to zero if > max or to max if < 0
#define AddNLoop(x,add,max) ((x)==(max)&&(add)?0:((x)==0&&!(add)?(max):((add)?(x)+1:(x)-1)))


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
	uint8_t mode : 4;		//	CV or pitch mode
	uint8_t root : 6;
	uint8_t scale : 6;
	struct CvStep Steps[8];
};
struct GateSequence {
	uint8_t steps : 4;		//  Number of steps in sequence
	uint8_t mode : 4;		//	Gate or trigger mode
	struct GateStep Steps[8];
};

struct CvPatterns {
	struct CvSequence seq[8];
};
struct GatePatterns {
	struct GateSequence seq[8];
};


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

