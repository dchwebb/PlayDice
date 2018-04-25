#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Encoder.h>
#include "Adafruit_SSD1306.h"
#include "ClockHandler.h"
#include "DisplayHandler.h"
#include "Settings.h"
//#include <vector>
#include <algorithm>
//#include <stdio.h>

const boolean DEBUGCLOCK = 0;
const boolean DEBUGSTEPS = 0;
const boolean DEBUGRAND = 0;
const boolean DEBUGFRAME = 0;
const boolean DEBUGBTNS = 1;

//#define ENCODER_DO_NOT_USE_INTERRUPTS

uint16_t tempoPot = 512;		// Reading from tempo potentiometer for setting bpm
uint16_t bpm = 120;				// beats per minute of sequence (assume sequence runs in eighth notes for now)
uint16_t minBPM = 35;			// minimum BPM allowed for internal/external clock
uint16_t maxBPM = 300;			// maximum BPM allowed for internal/external clock
elapsedMillis timeCounter = 0;  // millisecond counter to check if next sequence step is due
elapsedMillis debugCounter = 0;	// used to show debug data only every couple of ms
unsigned long stepStart;		// time each new step starts
uint32_t guessNextStep;			// guesstimate of when next step will fall - to avoid display firing at wrong time
uint32_t lastEditing = 0;		// ms counter to show detailed edit parameters while editing or just after
uint8_t voltsMin = 0;			// Minimum allowed voltage amt per step
uint8_t voltsMax = 5;			// Maximum allowed voltage amt
float cvRandVal = 0;			// Voltage of current step with randomisation applied
boolean gateRandVal;			// 1 or 0 according to whether gate is high or low after randomisation
uint8_t cvStutterStep;			// if a step is in stutter mode store the count of the current stutters 
uint8_t gateStutterStep;		// if a step is in stutter mode store the count of the current stutters 
uint8_t cvSeqNo = 0;			// store the sequence number for CV patterns
uint8_t gateSeqNo = 0;			// store the sequence number for Gate patterns
uint8_t numSeqA = 8;			// number of sequences in A section (CV)
seqMode cvLoopMode = LOOPALL;	// loop mode (LOOPCURRENT, LOOPALL)
uint8_t numSeqB = 8;			// number of sequences in B section (gate)
seqMode gateLoopMode = LOOPCURRENT;	// loop mode (LOOPCURRENT, LOOPALL)
int8_t seqStep = -1;			// increments each step of sequence
int8_t editStep = 0;			// store which step is currently selected for editing (-1 = choose seq, 0-7 are the sequence steps)
editType editMode = STEPV;		// enum editType - eg editing voltage, random amts etc
seqType activeSeq = SEQCV;	// whether the CV or Gate rows is active for editing
uint16_t clockBPM = 0;			// BPM read from external clock
long oldEncPos = 0;
boolean bothButtons;

//	declare variables
struct CvPatterns cv;
struct GatePatterns gate;
Btn btns[] = { { STEPUP, 1 },{ STEPDN, 0 },{ ENCODER, 2 },{ ACTION, 9 } };
int stutterArray[] = { 0, 2, 3, 4, 6, 8 };
Encoder myEnc(ENCCLKPIN, ENCDATAPIN);
ClockHandler clock(minBPM, maxBPM);
DisplayHandler dispHandler;

void initCvSequence(int seqNum, seqInitType initType, uint16_t numSteps = 8) {
	numSteps = (numSteps == 0 || numSteps > 8 ? 8 : numSteps);
	cv.seq[seqNum].steps = numSteps;
	for (int s = 0; s < 8; s++) {
		cv.seq[seqNum].Steps[s].volts = (initType == INITBLANK ? 2.5 : getRand() * 5);
		cv.seq[seqNum].Steps[s].rand_amt = (initType == INITBLANK ? 0 : round((getRand() * 10)));
		cv.seq[seqNum].Steps[s].stutter = 0;
	}
}
void initGateSequence(int seqNum, seqInitType initType, uint16_t numSteps = 8) {
	numSteps = (numSteps == 0 || numSteps > 8 ? 8 : numSteps);
	gate.seq[seqNum].steps = numSteps;
	for (int s = 0; s < 8; s++) {
		gate.seq[seqNum].Steps[s].on = (initType == INITBLANK ? 0 : round(getRand()));
		gate.seq[seqNum].Steps[s].rand_amt = 0; // (initType == INITBLANK ? 0 : round(getRand() * 10));
		gate.seq[seqNum].Steps[s].stutter = 0; // s == 0 ? 8 : 0;
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
	for (int b = 0; b < 4; b++) {
		pinMode(btns[b].pin, INPUT_PULLUP);
	}

	//  Set up CV and Gate patterns
	srand(micros());
	for (int p = 0; p < 8; p++) {
		initCvSequence(p, INITRAND);
		srand(micros());
		initGateSequence(p, INITRAND);
	}

	// initialiase encoder
	oldEncPos = round(myEnc.read() / 4);
	lastEditing = 0;		// this somehow gets set to '7' on startup - not sure how at this stage
	editMode = STEPV;		// this somehow gets set to '1' on startup
}

void loop() {
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
	}
	else {
		bpm = map(tempoPot, 0, 1023, minBPM, maxBPM);        // map(value, fromLow, fromHigh, toLow, toHigh)
	}

	//	check if the sequence counter is ready to advance to the next step. Also if using external clock wait for pulse
	uint16_t timeStep = 1000 / (((float)bpm / 60) * 2);		// get length of step based on bpm
	boolean newStep = (timeCounter >= timeStep && (!clock.hasSignal() || millis() - clock.clockHighTime < 10));

	boolean newStutter = 0;
	if (gateStutterStep > 0 && gateStutterStep < gate.seq[gateSeqNo].Steps[seqStep].stutter) {
		if (timeCounter >= gateStutterStep * (timeStep / gate.seq[gateSeqNo].Steps[seqStep].stutter)) {
			if (DEBUGSTEPS) { Serial.print("new stutter step: "); Serial.println(millis()); }
			newStutter = 1;
		}
	}

	if (newStep || newStutter) {
		//	increment sequence step and reinitialise stutter steps
		if (newStep) {
			seqStep += 1;
			if (seqStep == cv.seq[cvSeqNo].steps) {
				seqStep = 0;
				if (cvLoopMode == LOOPALL) {
					cvSeqNo = cvSeqNo++ >= 7 ? 0 : cvSeqNo;
					if (DEBUGSTEPS) { Serial.print("CV seq: "); Serial.println(cvSeqNo); }
				}
				if (gateLoopMode == LOOPALL) {
					gateSeqNo = gateSeqNo++ >= 7 ? 0 : gateSeqNo;
					if (DEBUGSTEPS) { Serial.print("Gate seq: "); Serial.println(gateSeqNo); }
				}
			}
			cvStutterStep = 0;
			gateStutterStep = 0;
			if (DEBUGSTEPS) { Serial.print("new step: "); Serial.print(seqStep); Serial.print(" millis: "); Serial.println(millis());	}
		}

		//	guess the next step or stutter time to estimate if we have time to do a refresh
		guessNextStep = millis() + (gate.seq[gateSeqNo].Steps[seqStep].stutter ? (timeStep / gate.seq[gateSeqNo].Steps[seqStep].stutter) : timeStep);
		if (DEBUGSTEPS) {
			Serial.print("guess next step: "); Serial.println(guessNextStep);
		}

		// CV sequence: calculate possible ranges of randomness to ensure we don't try and set a random value out of permitted range
		if (newStep || cvStutterStep > 0) {
			float randLower = getRandLimit(cv.seq[cvSeqNo].Steps[seqStep], LOWER);
			float randUpper = getRandLimit(cv.seq[cvSeqNo].Steps[seqStep], UPPER);
			cvRandVal = constrain(randLower + (getRand() * (randUpper - randLower)), 0, voltsMax);
			setCV(cvRandVal);
			if (DEBUGRAND) {
				Serial.print("CV  S: "); Serial.print(seqStep);	Serial.print(" V: "); Serial.print(cv.seq[cvSeqNo].Steps[seqStep].volts); Serial.print(" Rnd: "); Serial.println(cv.seq[cvSeqNo].Steps[seqStep].rand_amt);
				Serial.print("    Lwr: "); Serial.print(randLower); Serial.print(" Upr: "); Serial.print(randUpper); Serial.print(" Result: "); Serial.println(cvRandVal);
			}
		}

		// Gate sequence: calculate probability of gate being high or low. Eg rand_amt = 9 means there is a 90% chance that the value will be randomised
		if (newStep || gateStutterStep > 0) {

			if (gate.seq[gateSeqNo].Steps[seqStep].stutter > 0) {
				gateStutterStep += 1;
				gateRandVal = (gateStutterStep % 2 > 0);
			}
			else {
				if (gate.seq[gateSeqNo].Steps[seqStep].rand_amt) {
					uint8_t rndXTen = round(getRand() * 10);
					float r = getRand();
					gateRandVal = rndXTen < gate.seq[gateSeqNo].Steps[seqStep].rand_amt ? round(r) : gate.seq[gateSeqNo].Steps[seqStep].on;

					if (DEBUGRAND) {
						Serial.print("GT  on/off: "); Serial.print(gate.seq[gateSeqNo].Steps[seqStep].on); Serial.print(" prb: "); Serial.print(gate.seq[gateSeqNo].Steps[seqStep].rand_amt); Serial.print(" > rand: "); Serial.print(rndXTen);
						Serial.print(" Gate: "); Serial.print(r); Serial.print(" changed: "); Serial.println(gate.seq[gateSeqNo].Steps[seqStep].on != gateRandVal);
					}
				}
				else {
					gateRandVal = gate.seq[gateSeqNo].Steps[seqStep].on;
				}

			}
			digitalWrite(GATEOUT, gateRandVal);
		}

		// flash LED and reset step info
		if (newStep) {
			digitalWrite(LED, seqStep % 2 == 0 ? HIGH : LOW);
			timeCounter = 0;
			newStep = 0;
		}
		if (!gateStutterStep) {
			dispHandler.setDisplayRefresh(REFRESHFULL);
		}
	}

	// Handle Encoder turn - alter parameter depending on edit mode
	long newEncPos = myEnc.read();
	if (newEncPos != oldEncPos) {
		// check editing mode is valid for selected step type
		checkEditState();

		if (round(newEncPos / 4) != round(oldEncPos / 4)) {
			boolean upOrDown = newEncPos > oldEncPos;

			// change parameter
			if (editStep >= 0) {

				if (activeSeq == SEQCV) {
					CvStep *s = &cv.seq[cvSeqNo].Steps[editStep];
					if (editMode == STEPV) {
						s->volts += upOrDown ? 0.10 : -0.10;
						s->volts = constrain(s->volts, 0, 5);
						Serial.print("volts: "); Serial.print(s->volts);
					}
					if (editMode == STEPR && (upOrDown || s->rand_amt > 0) && (!upOrDown || s->rand_amt < 10)) {
						s->rand_amt += upOrDown ? 1 : -1;
						Serial.print("rand: "); Serial.print(s->rand_amt);
					}
					if (editMode == STUTTER && (upOrDown || s->stutter > 0) && (!upOrDown || s->stutter < 8)) {
						//	As stutter amounts are a fixed list of musical divisions use an array to increase/decrease
						int * p = std::find(stutterArray, stutterArray + sizeof(stutterArray), (int)s->stutter);
						s->stutter = stutterArray[std::distance(stutterArray, p) + (upOrDown ? 1 : -1)];
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
						//	As stutter amounts are a fixed list of musical divisions use an array to increase/decrease
						int * p = std::find(stutterArray, stutterArray + sizeof(stutterArray), (int)s->stutter);
						s->stutter = stutterArray[std::distance(stutterArray, p) + (upOrDown ? 1 : -1)];
						Serial.print("stutter: "); Serial.print(s->stutter);
					}
				}
			}
			else {

				//	sequence select mode
				if (editMode == PATTERN) {
					//uint8_t * pSeq;
					uint8_t * pSeq = activeSeq == SEQCV ? &cvSeqNo : &gateSeqNo;
					*pSeq += upOrDown ? 1 : -1;
					*pSeq = *pSeq < 1 ? 8 : (*pSeq > 8 ? 1 : *pSeq);
					if (DEBUGBTNS) { Serial.print("cv no: ");  Serial.print(cvSeqNo); Serial.print("gate: ");  Serial.print(gateSeqNo); }
				}
				if (editMode == SEQS) {
					numSeqA += upOrDown ? 1 : -1;
					numSeqA = constrain(numSeqA, 1, 8);
					if (DEBUGBTNS) { Serial.print("seqs: ");  Serial.print(numSeqA); }
				}

				if (editMode == SEQMODE) {
					if (activeSeq == SEQCV) {
						cvLoopMode = cvLoopMode == LOOPCURRENT ? LOOPALL : LOOPCURRENT;
						if (DEBUGBTNS) { Serial.print("mode: ");  Serial.print(cvLoopMode); }
					}
					else {
						gateLoopMode = gateLoopMode == LOOPCURRENT ? LOOPALL : LOOPCURRENT;
						if (DEBUGBTNS) { Serial.print("mode: ");  Serial.print(gateLoopMode); }
					}
				}
			}
			if (DEBUGBTNS) Serial.print("  Encoder: ");  Serial.println(newEncPos);
		}

		oldEncPos = newEncPos;
		lastEditing = millis();
		dispHandler.setDisplayRefresh(REFRESHFULL);
	}

	// handle momentary button presses - step up/down or encoder button to switch editing mode
	for (int b = 0; b < 4; b++) {
		//  Parameter button handler
		if (digitalRead(btns[b].pin)) {
			btns[b].pressed = 0;
		}
		else {
			//  check if button has been pressed (previous state off and over x milliseconds since last on)
			if (btns[b].pressed == 0 && millis() - btns[b].lastPressed > 10) {
				if (btns[b].name == STEPUP || btns[b].name == STEPDN) {
					editStep = editStep + (btns[b].name == STEPUP ? 1 : -1);
					editStep = (editStep > 7 ? -1 : (editStep < -1 ? 7 : editStep));

					if (editStep > -1) {
						lastEditing = millis();
					}

					// check editing mode is valid for selected step type
					checkEditState();

					if (DEBUGBTNS) {
						if (btns[STEPUP].pressed) Serial.println("Step up");
						if (btns[STEPDN].pressed) Serial.println("Step dn");
					}
				}
				if (btns[b].name == ACTION) {
					if (DEBUGBTNS) {
						Serial.println("Action");
						seqStep = 0;
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
							editMode = SEQMODE;
							break;
						case SEQMODE:
							editMode = SEQS;
							break;
						case SEQS:
							editMode = PATTERN;
							break;
						}
					}
					else {
						editMode = editStep == -1 ? PATTERN : STEPV;
					}


					lastEditing = millis();
					if (DEBUGBTNS)
						Serial.println("Encoder button");
				}

			}
			btns[b].pressed = 1;
			btns[b].lastPressed = millis();
		}
	}

	//	if both buttons pressed at the same time switch between gate and cv patterns
	if (btns[STEPUP].pressed && btns[STEPDN].pressed) {
		if (bothButtons == 0) {
			if (DEBUGBTNS)
				Serial.println(" BOTH");
			bothButtons = 1;
			activeSeq = (activeSeq == SEQGATE ? SEQCV : SEQGATE);
			lastEditing = 0;
		}
	}
	else {
		bothButtons = 0;
	}

	//	Trigger a display refresh if the clock has just received a signal or not received one for over a second (to avoid display interfering with clock reading)
	/*if (dispHandler.displayRefresh > 0 && millis() > 1000 && (millis() - clock.clockHighTime < 10 || millis() - clock.clockHighTime > 1000)) {
		dispHandler.updateDisplay();
	}*/

	// about the longest display update time is 24 milliseconds so don't update display if less than 24 milliseconds until the next expected event (step change or clock tick)
	if (millis() > 1000 && guessNextStep - millis() > 24 && clock.clockHighTime + clock.clockInterval - millis() > 24) {
		dispHandler.updateDisplay();
	}
}


void setCV(float setVolt) {
	//  DAC buffer takes values of 0 to 4095 relating to 0v to 3.3v
	//  setVolt will be in range 0 - voltsMax (5 unless trying to do pitch which might need negative)
	float dacVolt = setVolt / voltsMax * 4095;
	analogWrite(DACPIN, (int)dacVolt);

}


boolean checkEditing() {
	// check if recent encoder activity
	return (lastEditing > 1 && millis() - lastEditing < 5000);
}

void checkEditState() {
	// check editing mode is valid for selected step type
	if (editStep == -1 && (editMode == STEPV || editMode == STEPR || editMode == STUTTER)) {
		editMode = PATTERN;
	}
	if (editStep > -1 && (editMode == PATTERN || editMode == SEQS || editMode == SEQMODE)) {
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

