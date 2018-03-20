#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Encoder.h>
#include "Adafruit_SSD1306.h"
#include "ClockHandler.h"
#include "DisplayHandler.h"
#include "Settings.h"
//#include <stdio.h>

const boolean DEBUGCLOCK = 0;
const boolean DEBUGSTEPS = 0;
const boolean DEBUGFRAME = 0;
const boolean DEBUGBTNS = 1;

//#define ENCODER_DO_NOT_USE_INTERRUPTS

uint16_t tempoPot = 512;		// Reading from tempo potentiometer for setting bpm
uint16_t bpm = 120;				// beats per minute of sequence (assume sequence runs in eighth notes for now)
uint16_t minBPM = 35;			// minimum BPM allowed for internal/external clock
uint16_t maxBPM = 300;			// maximum BPM allowed for internal/external clock
elapsedMillis timeCounter = 0;  // millisecond counter to check if next sequence step is due
elapsedMillis debugCounter = 0;	// used to show debug data only every couple of ms
uint32_t lastEditing = 0;		// ms counter to show detailed edit parameters while editing or just after
uint8_t voltsMin = 0;			// Minimum allowed voltage amt per step
uint8_t voltsMax = 5;			// Maximum allowed voltage amt
float cvRandVal = 0;				// Voltage of current step with randomisation applied
boolean gateRandVal;			// 1 or 0 according to whether gate is high or low after randomisation
uint8_t cvSeqNo = 1;			// store the sequence number for CV patterns
uint8_t gateSeqNo = 1;			// store the sequence number for Gate patterns
uint8_t numSeqA = 8;			// number of sequences in A section (S&H)
seqMode modeSeqA = LOOPCURRENT;	// loop mode (LOOPCURRENT, LOOPALL)
uint8_t numSeqB = 8;			// number of sequences in B section (gate)
seqMode modeSeqB = LOOPCURRENT;	// loop mode (LOOPCURRENT, LOOPALL)
int8_t seqStep = -1;			// increments each step of sequence
int8_t editStep = 0;			// store which step is currently selected for editing (-1 = choose seq, 0-7 are the sequence steps)
editType editMode = STEPV;		// enum editType - eg editing voltage, random amts etc
seqType activeSeq = SEQGATE;	// whether the CV or Gate rows is active for editing
uint16_t clockBPM = 0;			// BPM read from external clock
long oldEncPos = 0;
boolean bothButtons;

//	declare variables
struct CvPatterns cv;
struct GatePatterns gate;
Btn btns[] = { { STEPUP, 12 },{ STEPDN, 11 },{ ENCODER, 10 } };
Encoder myEnc(ENCCLKPIN, ENCDATAPIN);
ClockHandler clock(minBPM, maxBPM);
DisplayHandler dispHandler;

void initCvSequence(int seqNum, seqInitType initType, uint16_t numSteps = 8) {
	numSteps = (numSteps == 0 || numSteps > 8 ? 8 : numSteps);
	cv.seq[seqNum].steps = numSteps;
	for (int s = 0; s < 8; s++) {
		cv.seq[seqNum].Steps[s].volts = (initType == INITBLANK ? 2.5 : getRand() * 5);
		cv.seq[seqNum].Steps[s].rand_amt = (initType == INITBLANK ? 0 : round((getRand() * 10)));
		cv.seq[seqNum].Steps[s].stutter = (initType == INITBLANK ? 0 : round(getRand()));
	}
}
void initGateSequence(int seqNum, seqInitType initType, uint16_t numSteps = 8) {
	numSteps = (numSteps == 0 || numSteps > 8 ? 8 : numSteps);
	gate.seq[seqNum].steps = numSteps;
	for (int s = 0; s < 8; s++) {
		gate.seq[seqNum].Steps[s].on = (initType == INITBLANK ? 0 : round(getRand()));
		gate.seq[seqNum].Steps[s].rand_amt = (initType == INITBLANK ? 0 : round(getRand() * 10));
		gate.seq[seqNum].Steps[s].stutter = (initType == INITBLANK ? 0 : round(getRand()));
	}
}

double getRand() {
	return (double)rand() / (double)RAND_MAX;
}


void setup() {

	pinMode(LED, OUTPUT);
	analogWriteResolution(12);    // set resolution of DAC pin for outputting variable voltages

	// Setup OLED
	dispHandler.init();

	//	initialise all momentary buttons as Input pullup
	for (int b = 0; b < 3; b++) {
		pinMode(btns[b].pin, INPUT_PULLUP);
	}

	//  Set up CV and Gate patterns
	srand(micros());
	for (int p = 1; p <= 8; p++) {
		initCvSequence(p, INITRAND);
		srand(micros());
		initGateSequence(p, INITRAND);
	}

	//Serial.print("int: ");  Serial.println(sizeof(minBPM));

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
	if (timeCounter >= timeStep && (!clock.hasSignal() || millis() - clock.clockHighTime < 10)) {

		//	increment sequence step
		seqStep += 1;
		if (seqStep == cv.seq[cvSeqNo].steps) {
			seqStep = 0;
		}

		
		// CV sequence: calculate possible ranges of randomness to ensure we don't try and set a random value out of permitted range
		float randLower = getRandLimit(cv.seq[cvSeqNo].Steps[seqStep], LOWER);
		float randUpper = getRandLimit(cv.seq[cvSeqNo].Steps[seqStep], UPPER);
		cvRandVal = constrain(randLower + (getRand() * (randUpper - randLower)), 0, voltsMax);
		setCV(cvRandVal);

		if (DEBUGSTEPS) {
			Serial.print("S: "); Serial.println(seqStep);
			Serial.print("V: "); Serial.print(cv.seq[cvSeqNo].Steps[seqStep].volts); Serial.print(" R: "); Serial.println(cv.seq[cvSeqNo].Steps[seqStep].rand_amt);
			Serial.print("L: "); Serial.print(randLower); Serial.print(" U: "); Serial.println(randUpper);
			Serial.print("result: "); Serial.println(cvRandVal);
		}

		// Gate sequence: calculate probability of gate being high or low. Eg rand_amt = 9 means there is a 90% chance that the value will be randomised
		uint8_t rndXTen = round(getRand() * 10);
		float r = getRand();
		if (rndXTen < gate.seq[gateSeqNo].Steps[seqStep].rand_amt) {
			
			gateRandVal = round(r);
		}
		else {
			gateRandVal = gate.seq[gateSeqNo].Steps[seqStep].on;
		}
		if (rndXTen < gate.seq[gateSeqNo].Steps[seqStep].rand_amt) {
			Serial.print("rndXTen: "); Serial.print(rndXTen);
			Serial.print(" r: "); Serial.print(r);
			Serial.print(" changed: "); Serial.println(gate.seq[gateSeqNo].Steps[seqStep].on != gateRandVal);
		}

		digitalWrite(LED, seqStep % 2 == 0 ? HIGH : LOW);
		timeCounter = 0;

		dispHandler.setDisplayRefresh(REFRESHFULL);
	}

	// Handle Encoder turn - alter parameter depending on edit mode
	long newEncPos = myEnc.read();
	if (newEncPos != oldEncPos) {
		// check editing mode is valid for selected step type
		checkEditState();

		if (round(newEncPos / 4) != round(oldEncPos / 4)) {
			// change parameter
			if (editStep >= 0) {
				if (activeSeq == SEQCV) {
					if (editMode == STEPV) {
						cv.seq[cvSeqNo].Steps[editStep].volts += newEncPos > oldEncPos ? 0.10 : -0.10;
						cv.seq[cvSeqNo].Steps[editStep].volts = constrain(cv.seq[cvSeqNo].Steps[editStep].volts, 0, 5);
						Serial.print("volts: ");  Serial.print(cv.seq[cvSeqNo].Steps[editStep].volts);
					}
					if (editMode == STEPR) {
						cv.seq[cvSeqNo].Steps[editStep].rand_amt += newEncPos > oldEncPos ? 1 : -1;
						cv.seq[cvSeqNo].Steps[editStep].rand_amt = constrain(cv.seq[cvSeqNo].Steps[editStep].rand_amt, (uint16_t)0, (uint16_t)10);
						Serial.print("rand: ");  Serial.print(cv.seq[cvSeqNo].Steps[editStep].rand_amt);
					}
				}
				else {
					if (editMode == STEPV) {
						gate.seq[gateSeqNo].Steps[editStep].on = !gate.seq[gateSeqNo].Steps[editStep].on;
						Serial.print("on: ");  Serial.print(gate.seq[gateSeqNo].Steps[editStep].on);
					}
					if (editMode == STEPR) {
						gate.seq[gateSeqNo].Steps[editStep].rand_amt += newEncPos > oldEncPos ? 1 : -1;
						gate.seq[gateSeqNo].Steps[editStep].rand_amt = constrain(gate.seq[gateSeqNo].Steps[editStep].rand_amt, (uint16_t)0, (uint16_t)10);
						Serial.print("rand: ");  Serial.print(gate.seq[gateSeqNo].Steps[editStep].rand_amt);
					}
				}
			}
			else {

				//	sequence select mode
				if (editMode == PATTERN) {
					//uint8_t * pSeq;
					uint8_t * pSeq = activeSeq == SEQCV ? &cvSeqNo : &gateSeqNo;
					*pSeq += newEncPos > oldEncPos ? 1 : -1;
					*pSeq = *pSeq < 1 ? 8 : (*pSeq > 8 ? 1 : *pSeq);
					if (DEBUGBTNS) Serial.print("cv no: ");  Serial.print(cvSeqNo); Serial.print("gate: ");  Serial.print(gateSeqNo);
				}
				if (editMode == SEQS) {
					numSeqA += newEncPos > oldEncPos ? 1 : -1;
					numSeqA = constrain(numSeqA, 1, 8);
					if (DEBUGBTNS) Serial.print("seqs: ");  Serial.print(numSeqA);
				}

				if (editMode == SEQMODE) {
					modeSeqA = modeSeqA == LOOPCURRENT ? LOOPALL : LOOPCURRENT;
					if (DEBUGBTNS) Serial.print("mode: ");  Serial.print(modeSeqA);
				}
			}
		}

		oldEncPos = newEncPos;
		if (DEBUGBTNS) Serial.print("  Encoder: ");  Serial.println(newEncPos);
		lastEditing = millis();
		dispHandler.setDisplayRefresh(REFRESHFULL);
	}

	// handle momentary button presses - step up/down or encoder button to switch editing mode
	for (int b = 0; b < 3; b++) {
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
	if (dispHandler.displayRefresh > 0 && millis() > 1000 && (millis() - clock.clockHighTime < 10 || millis() - clock.clockHighTime > 1000)) {
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

