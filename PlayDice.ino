#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Encoder.h>
#include "Adafruit_SSD1306.h"
#include "ClockHandler.h"
#include "DisplayHandler.h"
#include "SetupFunctions.h"
#include "Settings.h"
#include <array>
#include <algorithm>

const boolean DEBUGCLOCK = 0;
const boolean DEBUGSTEPS = 0;
const boolean DEBUGRAND = 0;
const boolean DEBUGFRAME = 0;
const boolean DEBUGBTNS = 1;

uint16_t tempoPot = 512, oldTempoPot = 0;		// Reading from tempo potentiometer for setting bpm
uint16_t bpm = 120;				// beats per minute of sequence (assume sequence runs in eighth notes for now)
uint16_t minBPM = 35;			// minimum BPM allowed for internal/external clock
uint16_t maxBPM = 300;			// maximum BPM allowed for internal/external clock
elapsedMillis timeCounter = 0;  // millisecond counter to check if next sequence step is due
elapsedMillis debugCounter = 0;	// used to show debug data only every couple of ms
unsigned long stepStart;		// time each new step starts
uint32_t guessNextStep;			// guesstimate of when next step will fall - to avoid display firing at wrong time
uint32_t lastEditing = 0;		// ms counter to show detailed edit parameters while editing or just after
boolean saveRequired;			// set to true after editing a parameter needing a save (saves batched to avoid too many writes)
boolean autoSave = 1;			// set to true if autosave enabled
float cvRandVal = 0;			// Voltage of current step with randomisation applied
boolean gateRandVal;			// 1 or 0 according to whether gate is high or low after randomisation
uint8_t cvStutterStep;			// if a step is in stutter mode store the count of the current stutters 
uint8_t gateStutterStep;		// if a step is in stutter mode store the count of the current stutters 
uint8_t cvSeqNo = 0;			// store the sequence number for CV patterns
uint8_t gateSeqNo = 0;			// store the sequence number for Gate patterns
uint8_t cvLoopFirst = 0;		// first sequence in loop
uint8_t cvLoopLast = 0;			// last sequence in loop
uint8_t gateLoopFirst = 0;		// first sequence in loop
uint8_t gateLoopLast = 0;		// last sequence in loop
int8_t cvStep = -1;				// increments each step of cv sequence
int8_t gateStep = -1;			// increments each step of gate sequence
int8_t editStep = 0;			// store which step is currently selected for editing (-1 = choose seq, 0-7 are the sequence steps)
editType editMode = STEPV;		// enum editType - eg editing voltage, random amts etc
seqType activeSeq = SEQCV;		// whether the CV or Gate rows is active for editing
uint16_t clockBPM = 0;			// BPM read from external clock
long oldEncPos = 0;
boolean actionStutter;			// Stutter triggered by action button
uint8_t stutterStep;			// When stutter is triggered by action button store stutter step number based on current clock speed 
uint8_t actionStutterNo = 8;	// Number of stutter steps when triggered by action button
float lfoX = 1, lfoY = 0;		// LFO parameters for quick Minsky approximation
float lfoSpeed;					// lfoSpeed calculated from tempo pot
float oldLfoSpeed;				// lfoSpeed calculated from tempo pot
uint8_t lfoJitter;				// because the analog pot is more sensitive at the bottom of its range adjust the threshold before detecting a pot turn
boolean pitchMode;				// set to true if CV lane displays and quantises to pitches
elapsedMillis lfoCounter = 0;	// millisecond counter to check if next lfo calculation is due

//	declare variables
struct CvPatterns cv;
struct GatePatterns gate;
Btn btns[] = { { STEPDN, 22 },{ STEPUP, 12 },{ ENCODER, 15 },{ CHANNEL, 19 },{ ACTION, 20 },{ ACTIONCV, 21 } };		// numbers refer to Teensy digital pin numbers

Encoder myEnc(ENCCLKPIN, ENCDATAPIN);
ClockHandler clock(minBPM, maxBPM);
DisplayHandler dispHandler;
SetupMenu setupMenu;
actionOpts actionType = ACTSTUTTER;

void initCvSequence(int seqNum, seqInitType initType, uint16_t numSteps = 8) {
	numSteps = (numSteps == 0 || numSteps > 8 ? 8 : numSteps);
	cv.seq[seqNum].steps = numSteps;
	for (int s = 0; s < 8; s++) {
		cv.seq[seqNum].Steps[s].volts = (initType == INITBLANK ? 2.5 : getRand() * 5);
		cv.seq[seqNum].Steps[s].rand_amt = (initType == INITRAND ? round((getRand() * 10)) : 0);
		//	Don't want too many stutters so apply two random checks to see if apply stutter, and if so how much - minimum number of stutters is 2
		if (initType == INITRAND && getRand() > 0.8) {
			cv.seq[seqNum].Steps[s].stutter = round((getRand() * 6) + 1);
		}
		else {
			cv.seq[seqNum].Steps[s].stutter = 0;
		}
	}
}
void initGateSequence(int seqNum, seqInitType initType, uint16_t numSteps = 8) {
	numSteps = (numSteps == 0 || numSteps > 8 ? 8 : numSteps);
	gate.seq[seqNum].steps = numSteps;
	for (int s = 0; s < 8; s++) {
		gate.seq[seqNum].Steps[s].on = (initType == INITBLANK ? 0 : round(getRand()));
		gate.seq[seqNum].Steps[s].rand_amt = (initType == INITRAND ? round((getRand() * 10)) : 0);
		//	Don't want too many stutters so apply two random checks to see if apply stutter, and if so how much
		if (initType == INITRAND && getRand() > 0.8) {
			gate.seq[seqNum].Steps[s].stutter = round((getRand() * 6) + 1);
		}
		else {
			gate.seq[seqNum].Steps[s].stutter = 0;
		}
	}
}

double getRand() {
	return (double)rand() / (double)RAND_MAX;
}


void setup() {

	pinMode(LED, OUTPUT);
	pinMode(GATEOUT, OUTPUT);
	pinMode(CLOCKPIN, INPUT);

	analogWriteResolution(12);    // set resolution of DAC pin for outputting variable voltages

	// Setup OLED
	dispHandler.init();

	//	initialise all momentary buttons as Input pullup
	for (int b = 0; b < 6; b++) {
		pinMode(btns[b].pin, INPUT_PULLUP);
	}

	if (!setupMenu.loadSettings()) {
		//  Set up CV and Gate patterns
		srand(micros());
		for (int p = 0; p < 8; p++) {
			initCvSequence(p, INITRAND);
			srand(micros());
			initGateSequence(p, INITRAND);
		}
	}

	/*
	// test scale settings - values are voltages for a 2 octave major scale
	cv.seq[0].Steps[0].volts = 2;
	cv.seq[0].Steps[1].volts = 2.167;
	cv.seq[0].Steps[2].volts = 2.333;
	cv.seq[0].Steps[3].volts = 2.417;
	cv.seq[0].Steps[4].volts = 2.583;
	cv.seq[0].Steps[5].volts = 2.75;
	cv.seq[0].Steps[6].volts = 2.917;
	cv.seq[0].Steps[7].volts = 3;
	cv.seq[1].Steps[0].volts = 3;
	cv.seq[1].Steps[1].volts = 3.167;
	cv.seq[1].Steps[2].volts = 3.333;
	cv.seq[1].Steps[3].volts = 3.417;
	cv.seq[1].Steps[4].volts = 3.583;
	cv.seq[1].Steps[5].volts = 3.75;
	cv.seq[1].Steps[6].volts = 3.917;
	cv.seq[1].Steps[7].volts = 4;
	*/

	// initialiase encoder
	oldEncPos = round(myEnc.read() / 4);

	if (editMode == LFO || editMode == NOISE) {
		dispHandler.updateDisplay();
	}
}

void loop() {

	if (editMode == LFO || editMode == NOISE) {

		if (digitalRead(btns[3].pin) == 0 || digitalRead(btns[4].pin) == 0) {
			normalMode();
			if (autoSave) {
				setupMenu.saveSettings();
			}
			return;
		}

		tempoPot = analogRead(TEMPOPIN);
		if (oldTempoPot - tempoPot > lfoJitter || tempoPot - oldTempoPot > lfoJitter || lfoSpeed < 0.0001) {
			//lfoSpeed = (float)cbrt(pow((float)tempoPot, 2)) / 50000;
			lfoSpeed = (float)(pow((float)tempoPot, (float)0.72)) / 50000;
			if (lfoSpeed < 0.0001) {
				lfoSpeed = 0.0001;
			}
			Serial.println(lfoSpeed * 1000);//lfoSpeed * 1000
			oldTempoPot = tempoPot;
			if (tempoPot < 6) {
				lfoJitter = 1;
			} else if (tempoPot < 20) {
				lfoJitter = 6;
			} else if (tempoPot < 300) {
				lfoJitter = 10;
			} else if (tempoPot < 700) {
				lfoJitter = 15;
			} else {
				lfoJitter = 30;
			}
		}

		lfoX -= lfoSpeed * lfoY;
		lfoY += lfoSpeed * lfoX;
		if (lfoY < -1) {
			lfoY = -1;
		} else if (lfoY > 1) {
			lfoY = 1;
		}

		//  DAC buffer takes values of 0 to 4095 relating to 0v to 3.3v
		if (editMode == LFO) {
			analogWrite(DACPIN, round(2047 * (lfoY + 1)));
		}
		else {
			analogWrite(DACPIN, round(4095 * getRand()));
		}
		digitalWrite(GATEOUT, lfoY > 0);
		
		return;
	}


	//	read value of clock signal if present and set bmp accordingly
	clockBPM = clock.readClock();

	if (DEBUGCLOCK && debugCounter > 5) {
		debugCounter = 0;
		clock.printDebug();
	}
	tempoPot = analogRead(TEMPOPIN);		//  read value of potentiometer to set speed

	// work out whether to get bpm from tempo potentiometer or clock signal (checking that we have recieved a recent clock signal)
	if (clockBPM >= minBPM && clockBPM < maxBPM && clock.hasSignal()) {
		bpm = clockBPM;
		// basic clock divider allowing tempo to be halved or doubled depending on position of tempo pot
		if (tempoPot < 341) {
			bpm = clockBPM / 2;
		} else if (tempoPot > 683) {
			bpm = clockBPM * 2;
		}
		//Serial.print("cl bpm: ");  Serial.print(clockBPM); Serial.print(" tp: ");  Serial.print(tempoPot); Serial.print(" bpm: ");  Serial.println(bpm);
	}
	else {
		bpm = map(tempoPot, 0, 1023, minBPM, maxBPM);        // map(value, fromLow, fromHigh, toLow, toHigh)
	}

	//	check if the sequence counter is ready to advance to the next step. Also if using external clock wait for pulse
	uint16_t timeStep = 1000 / (((float)bpm / 60) * 2);		// get length of step based on bpm
	boolean newStep = (timeCounter >= timeStep && (!clock.hasSignal() || millis() - clock.clockHighTime < 10));

	boolean newStutter = 0;
	if (gateStutterStep > 0 && gateStutterStep < gate.seq[gateSeqNo].Steps[gateStep].stutter) {
		if (timeCounter >= gateStutterStep * (timeStep / gate.seq[gateSeqNo].Steps[gateStep].stutter)) {
			if (DEBUGSTEPS) { Serial.print("new stutter step: ");  Serial.print(gateStutterStep);  Serial.print(" ms "); Serial.println(millis()); }
			newStutter = 1;
		}
	}
	if (cvStutterStep > 0 && cvStutterStep < cv.seq[cvSeqNo].Steps[cvStep].stutter) {
		if (timeCounter >= cvStutterStep * (timeStep / cv.seq[cvSeqNo].Steps[cvStep].stutter)) {
			//if (DEBUGSTEPS) { Serial.print("new stutter step: "); Serial.println(millis()); }
			newStutter = 1;
		}
	}

	//	if action button triggers a stutter check which step to activate
	if (actionStutter) {
		//if (DEBUGBTNS) { Serial.print("actionStutter: "); Serial.println(actionStutter); }
		//	divide current step length by stutter count - for now actionStutterNo
		if (stutterStep == 0) {
			stutterStep = (timeCounter / (timeStep / actionStutterNo)) + 1;
			//Serial.print("tc: "); Serial.print(timeCounter);  Serial.print(" ts ");  Serial.println(timeStep);
		}
		if (timeCounter >= stutterStep * (timeStep / actionStutterNo) && stutterStep < actionStutterNo) {
			if (DEBUGSTEPS) { Serial.print("action stutter step: "); Serial.print(stutterStep);  Serial.print(" ms ");  Serial.println(millis()); }
			newStutter = 1;
		}
	}
	else {
		stutterStep = 0;
	}


	if (newStep || newStutter) {
		//	increment sequence step and reinitialise stutter steps
		if (newStep) {
			cvStep += 1;
			if (cvStep >= cv.seq[cvSeqNo].steps) {
				cvStep = 0;
				if (!checkEditing() && cvLoopLast > cvLoopFirst) {
					cvSeqNo = cvSeqNo++ >= cvLoopLast ? cvLoopFirst : cvSeqNo;
					if (DEBUGSTEPS) { Serial.print("CV seq: "); Serial.println(cvSeqNo); }
				}
			}
			gateStep += 1;
			if (gateStep >= gate.seq[gateSeqNo].steps) {
				gateStep = 0;
				if (!checkEditing() && gateLoopLast > gateLoopFirst) {
					gateSeqNo = gateSeqNo++ >= gateLoopLast ? gateLoopFirst : gateSeqNo;
					if (DEBUGSTEPS) { Serial.print("Gate seq: "); Serial.println(gateSeqNo); }
				}
			}
			cvStutterStep = 0;
			gateStutterStep = 0;
			stutterStep = 0;
			if (DEBUGSTEPS) { Serial.print("new step: "); Serial.print(cvStep); Serial.print(" millis: "); Serial.println(millis()); }
		}

		//	guess the next step or stutter time to estimate if we have time to do a refresh
		guessNextStep = millis() + min(min((gate.seq[gateSeqNo].Steps[gateStep].stutter ? (timeStep / gate.seq[gateSeqNo].Steps[gateStep].stutter) : timeStep),
			(cv.seq[cvSeqNo].Steps[cvStep].stutter ? (timeStep / cv.seq[cvSeqNo].Steps[cvStep].stutter) : timeStep)),
			(actionStutter ? (timeStep / actionStutterNo) : timeStep)
		);

		if (DEBUGSTEPS) {
			Serial.print("guess next step: "); Serial.println(guessNextStep);
		}

		// CV sequence: calculate possible ranges of randomness to ensure we don't try and set a random value out of permitted range
		if (newStep || cvStutterStep > 0) {
			if (cv.seq[cvSeqNo].Steps[cvStep].stutter > 0 || actionStutter) {
				if (actionStutter) {
					cvStutterStep = stutterStep;
					stutterStep += 1;
				}
				cvStutterStep += 1;
			}
			float randLower = getRandLimit(cv.seq[cvSeqNo].Steps[cvStep], LOWER);
			float randUpper = getRandLimit(cv.seq[cvSeqNo].Steps[cvStep], UPPER);
			cvRandVal = constrain(randLower + (getRand() * (randUpper - randLower)), 0, 5);
			setCV(cvRandVal);
			if (DEBUGRAND) {
				Serial.print("CV  S: "); Serial.print(cvStep);	Serial.print(" V: "); Serial.print(cv.seq[cvSeqNo].Steps[cvStep].volts); Serial.print(" Rnd: "); Serial.println(cv.seq[cvSeqNo].Steps[cvStep].rand_amt);
				Serial.print("    Lwr: "); Serial.print(randLower); Serial.print(" Upr: "); Serial.print(randUpper); Serial.print(" Result: "); Serial.println(cvRandVal);
			}
		}

		// Gate sequence: calculate probability of gate being high or low. Eg rand_amt = 9 means there is a 90% chance that the value will be randomised
		if (newStep || gateStutterStep > 0 || actionStutter) {

			if (gate.seq[gateSeqNo].Steps[gateStep].stutter > 0 || actionStutter) {
				if (actionStutter) {
					gateStutterStep = stutterStep;
					stutterStep += 1;
				}
				gateStutterStep += 1;
				gateRandVal = (gateStutterStep % 2 > 0);
			}
			else {
				if (gate.seq[gateSeqNo].Steps[gateStep].rand_amt) {
					uint8_t rndXTen = round(getRand() * 10);
					float r = getRand();
					gateRandVal = rndXTen < gate.seq[gateSeqNo].Steps[gateStep].rand_amt ? round(r) : gate.seq[gateSeqNo].Steps[gateStep].on;

					if (DEBUGRAND) {
						Serial.print("GT  on/off: "); Serial.print(gate.seq[gateSeqNo].Steps[gateStep].on); Serial.print(" prb: "); Serial.print(gate.seq[gateSeqNo].Steps[gateStep].rand_amt); Serial.print(" > rand: "); Serial.print(rndXTen);
						Serial.print(" Gate: "); Serial.print(r); Serial.print(" changed: "); Serial.println(gate.seq[gateSeqNo].Steps[gateStep].on != gateRandVal);
					}
				}
				else {
					gateRandVal = gate.seq[gateSeqNo].Steps[gateStep].on;
				}

			}
			digitalWrite(GATEOUT, gateRandVal);
		}

		// flash LED and reset step info
		if (newStep) {
			digitalWrite(LED, cvStep % 2 == 0 ? HIGH : LOW);
			timeCounter = 0;
			newStep = 0;
		}
	}

	// Handle Encoder turn - alter parameter depending on edit mode
	long newEncPos = myEnc.read();
	if (newEncPos != oldEncPos) {

		// check editing mode is valid for selected step type
		checkEditState();
		//Serial.print("edit state: ");  Serial.println(editMode);
		if (round(newEncPos / 4) != round(oldEncPos / 4)) {
			boolean upOrDown = newEncPos > oldEncPos;

			if (editMode == SETUP) {
				setupMenu.menuPicker(upOrDown ? ENCUP : ENCDN);
			}
			else {

				// change parameter
				if (editStep >= 0) {

					if (activeSeq == SEQCV) {
						CvStep *s = &cv.seq[cvSeqNo].Steps[editStep];
						if (editMode == STEPV) {
							if (pitchMode) {
								s->volts = quantiseVolts(s->volts + (upOrDown ? 0.08333 : -0.08333));
							}
							else {
								s->volts += upOrDown ? 0.10 : -0.10;
							}
							s->volts = constrain(s->volts, 0, 5);
							Serial.print("volts: "); Serial.print(s->volts);
						}
						if (editMode == STEPR && (upOrDown || s->rand_amt > 0) && (!upOrDown || s->rand_amt < 10)) {
							s->rand_amt += upOrDown ? 1 : -1;
							Serial.print("rand: "); Serial.print(s->rand_amt);
						}
						if (editMode == STUTTER && (upOrDown || s->stutter > 0) && (!upOrDown || s->stutter < 8)) {
							s->stutter += upOrDown ? (s->stutter == 0 ? 2 : 1) : (s->stutter == 2 ? -2 : -1);
							Serial.print("stutter: "); Serial.print(s->stutter);
						}
					}
					else {
						GateStep *s = &gate.seq[gateSeqNo].Steps[editStep];
						if (editMode == STEPV) {
							s->on = !s->on;
							Serial.print("on: "); Serial.print(s->on);
						}
						if (editMode == STEPR && (upOrDown || s->rand_amt > 0) && (!upOrDown || s->rand_amt < 10)) {
							s->rand_amt += upOrDown ? 1 : -1;
							Serial.print("rand: "); Serial.print(s->rand_amt);
						}
						if (editMode == STUTTER && (upOrDown || s->stutter > 0) && (!upOrDown || s->stutter < 8)) {
							s->stutter += upOrDown ? (s->stutter == 0 ? 2 : 1) : (s->stutter == 2 ? -2 : -1);
							Serial.print("stutter: "); Serial.print(s->stutter);
						}
					}
				}
				else {

					//	sequence select mode
					if (editMode == PATTERN) {
						uint8_t * pSeq = activeSeq == SEQCV ? &cvSeqNo : &gateSeqNo;
						*pSeq += upOrDown ? 1 : -1;
						*pSeq = *pSeq == 255 ? 7 : (*pSeq > 7 ? 0 : *pSeq);		// because we are using an unsigned int -1 goes to 255
						if (DEBUGBTNS) { Serial.print("cv pat: ");  Serial.print(cvSeqNo); Serial.print(" gate: ");  Serial.print(gateSeqNo); }
					}

					if (editMode == LOOPFIRST) {
						uint8_t * loopF = activeSeq == SEQCV ? &cvLoopFirst : &gateLoopFirst;
						uint8_t * loopL = activeSeq == SEQCV ? &cvLoopLast : &gateLoopLast;
						*loopF += upOrDown ? 1 : -1;
						*loopF = *loopF == 255 ? 7 : (*loopF > 7 ? 0 : *loopF);		// because we are using an unsigned int -1 goes to 255
						*loopL = constrain(*loopL, *loopF, 7);
						if (DEBUGBTNS) { Serial.print("First loop: ");  Serial.print(*loopF); }
					}


					if (editMode == LOOPLAST) {
						uint8_t * loopF = activeSeq == SEQCV ? &cvLoopFirst : &gateLoopFirst;
						uint8_t * loopL = activeSeq == SEQCV ? &cvLoopLast : &gateLoopLast;
						*loopL += upOrDown ? 1 : -1;
						*loopL = *loopL == 255 ? *loopF : (*loopL > 7 ? 7 : *loopL);		// because we are using an unsigned int -1 goes to 255
						*loopL = constrain(*loopL, *loopF, 7);
						if (DEBUGBTNS) { Serial.print("Last loop: ");  Serial.print(*loopL); }
					}

					//	Steps select mode
					if (editMode == STEPS) {
						if (activeSeq == SEQCV) {
							cv.seq[cvSeqNo].steps = constrain(cv.seq[cvSeqNo].steps + (upOrDown ? 1 : -1), 1, 8);
						} else {
							gate.seq[gateSeqNo].steps = constrain(gate.seq[gateSeqNo].steps + (upOrDown ? 1 : -1), 1, 8);
						}
					}

					//	Initialise/randomise sequence mode - simple menu system
					if ((editMode == SEQOPT && upOrDown) || (editMode == RANDVALS && !upOrDown)) {
						editMode = RANDALL;
					}
					else {
						if ((editMode == SEQOPT && !upOrDown) || (editMode == RANDALL && upOrDown)) {
							editMode = RANDVALS;
						}
						else {
							if ((editMode == RANDVALS && upOrDown) || (editMode == RANDALL && !upOrDown)) {
								editMode = SEQOPT;
							}
						}
					}

				}
				lastEditing = millis();
				saveRequired = 1;
			}
			if (DEBUGBTNS) { Serial.print("  Encoder: ");  Serial.println(newEncPos); }
		}

		oldEncPos = newEncPos;
	}


	for (int b = 0; b < 6; b++) {
		//  Parameter button handler - digitalRead returns 0 when button down
		if (digitalRead(btns[b].pin)) {
			if (btns[b].pressed == 1) {
				btns[b].released = 1;
			}
			btns[b].pressed = 0;

			// Handle buttons that activate on release rather than click
			if (btns[b].released && btns[b].name == CHANNEL) {
				btns[b].released = 0;
				if (millis() - btns[b].lastPressed < 500) {
					if (DEBUGBTNS) Serial.println("Btn: Channel");
					if (editMode == SETUP) {
						normalMode();
					}
					else {
						activeSeq = (activeSeq == SEQGATE ? SEQCV : SEQGATE);
						lastEditing = 0;
					}
				}
			}

			if (btns[b].released && (btns[b].name == ACTION || btns[b].name == ACTIONCV)) {
				btns[b].released = 0;
				actionStutter = 0;
				if (DEBUGBTNS) Serial.println("Stutter off");
			}
		}
		else {
			//  check if button has been pressed (previous state off and over x milliseconds since last on)
			if (btns[b].pressed == 0 && millis() - btns[b].lastPressed > 10) {

				if (editMode == SETUP) {
					setupMenu.menuPicker(btns[b].name);
				}
				else {
					if (btns[b].name == STEPUP || btns[b].name == STEPDN) {
						editStep = editStep + (btns[b].name == STEPUP ? 1 : -1);
						editStep = (editStep > 7 ? -1 : (editStep < -1 ? 7 : editStep));

						if (editStep > -1 && (btns[b].name == STEPUP || btns[b].name == STEPDN || btns[b].name == ENCODER)) {
							lastEditing = millis();
						}

						// check editing mode is valid for selected step type
						checkEditState();

						if (DEBUGBTNS) {
							if (btns[STEPUP].pressed) Serial.println("Btn: Step up");
							if (btns[STEPDN].pressed) Serial.println("Btn: Step dn");
						}
					}

					if (btns[b].name == ACTION || btns[b].name == ACTIONCV) {
						if (DEBUGBTNS) { Serial.print("Btn: Action; type: "); Serial.println(actionType); }
						switch (actionType) {
						case ACTSTUTTER:
							actionStutter = 1;
							break;
						case ACTRESTART:
							cvStep = 0;
							gateStep = 0;
							break;
						}
					}


					if (btns[b].name == ENCODER) {

						if (checkEditing()) {
							switch (editMode) {
							case STEPV:
								editMode = STEPR;
								break;
							case STEPR:
								editMode = STUTTER;
								break;
							case STUTTER:
								editMode = STEPV;
								break;
							case PATTERN:
								editMode = STEPS;
								break;
							case STEPS:
								editMode = LOOPFIRST;
								break;
							case LOOPFIRST:
								editMode = LOOPLAST;
								break;
							case LOOPLAST:
								editMode = SEQOPT;
								break;
							case SEQOPT:
								editMode = STEPS;
								break;
							case RANDALL:
								if (activeSeq == SEQCV) {
									initCvSequence(cvSeqNo, INITRAND, cv.seq[cvSeqNo].steps);
								}
								else {
									initGateSequence(gateSeqNo, INITRAND, gate.seq[gateSeqNo].steps);
								}
								break;
							case RANDVALS:
								if (activeSeq == SEQCV) {
									initCvSequence(cvSeqNo, INITVALS, cv.seq[cvSeqNo].steps);
								}
								else {
									initGateSequence(gateSeqNo, INITVALS, gate.seq[gateSeqNo].steps);
								}
								break;
							case SETUP:
								break;
							case LFO:
								break;
							case NOISE:
								break;
							}
						}
						else {
							editMode = editStep == -1 ? STEPS : STEPV;
						}
						if (DEBUGBTNS) Serial.println("Btn: Encoder");
						lastEditing = millis();
					}

				}
				btns[b].lastPressed = millis();
			}
			btns[b].pressed = 1;
		}
	}

	//	Handle long click on the channel button to enter setup menu
	if (btns[CHANNEL].pressed && millis() - btns[CHANNEL].lastPressed > 500 && editMode != SETUP) {
		btns[CHANNEL].longClick = 1;
		editMode = SETUP;
		if (DEBUGBTNS) {
			Serial.println("Setup");
		}
	}

	// about the longest display update time is 2 milliseconds so don't update display if less than 5 milliseconds until the next expected event (step change or clock tick)
	uint32_t m = millis();
	if (m > 1000 && guessNextStep - m > 5 && clock.clockHighTime + clock.clockInterval - m > 5) {
		dispHandler.updateDisplay();
	}

	//	Check if there is a pending save and no edits in the last ten seconds
	m = millis();
	if (autoSave && saveRequired && m - lastEditing > 10000 && m > 1000 && guessNextStep - m > 5 && clock.clockHighTime + clock.clockInterval - m > 5) {
		Serial.println("Autosave triggered");
		setupMenu.saveSettings();
	}

}


void setCV(float setVolt) {
	//  DAC buffer takes values of 0 to 4095 relating to 0v to 3.3v
	//  setVolt will be in range 0 - voltsMax (5 unless trying to do pitch which might need negative)
	float dacVolt = setVolt / 5 * 4095;
	analogWrite(DACPIN, (int)dacVolt);
}


boolean checkEditing() {
	// check if recent encoder activity
	return (editMode != PATTERN && lastEditing > 1 && millis() - lastEditing < 5000);
}

void checkEditState() {
	// check editing mode is valid for selected step type
	if (editMode != SETUP) {
		if (editStep == -1 && (editMode == STEPV || editMode == STEPR || editMode == STUTTER || !checkEditing())) {
			editMode = PATTERN;
		}
		if (editStep > -1 && !(editMode == STEPV || editMode == STEPR || editMode == STUTTER)) {
			editMode = STEPV;
		}
	}
}

void normalMode() {
	// return to normal step or pattern editing after LFO mode, setup menu etc
	lastEditing = 0;
	if (editStep == -1) {
		editMode = PATTERN;
	} else {
		editMode = STEPV;
	}
}

float getRandLimit(CvStep s, rndType getUpper) {
	if (getUpper == UPPER) {
		return s.volts + ((double)s.rand_amt / 2);
	}
	else {
		return s.volts - ((double)s.rand_amt / 2);
	}
}

float quantiseVolts(float v) {
	return (float)round(v * 12) / 12;
}