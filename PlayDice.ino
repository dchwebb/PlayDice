#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Encoder.h>
#include "Adafruit_SSD1306.h"
#include "ClockHandler.h"
#include "DisplayHandler.h"
#include "Settings.h"
#include <stdio.h>

const boolean DEBUGCLOCK = 0;
const boolean DEBUGSTEPS = 1;
const boolean DEBUGFRAME = 0;

int tempoPot = 512;				// Reading from tempo potentiometer for setting bpm
int bpm = 120;                  // beats per minute of sequence (assume sequence runs in eighth notes for now)
int minBPM = 35;				// minimum BPM allowed for internal/external clock
int maxBPM = 300;				// maximum BPM allowed for internal/external clock
elapsedMillis timeCounter = 0;  // millisecond counter to check if next sequence step is due
elapsedMillis debugCounter = 0;	// used to show debug data only every couple of ms
long lastEncoder = 0;			// ms counter to show detailed edit parameters while editing or just after
int voltsMin = 0;               // Minimum allowed voltage amt per step
int voltsMax = 5;               // Maximum allowed voltage amt
float randAmt = 0;				// Voltage of current step with randomisation applied

int sequenceA = 1;				// store the sequence number for channel A (sample and hold)
int numSeqA = 8;				// number of sequences in A section (S&H)
int modeSeqA = 0;				// loop mode (LOOPCURRENT, LOOPALL)
int numSeqB = 8;				// number of sequences in B section (gate)
int sequenceB = 1;				// store the sequence number for channel B (gate)
int seqStep = -1;               // increments each step of sequence
int editStep = 0;				// store which step is currently selected for editing (-1 = choose seq, 0-7 are the sequence steps)
int editMode = STEPV;					// enum editType
int clockBPM = 0;				// BPM read from external clock
long oldEncPos = 0;

//	declare variables
struct Sequence seq;
struct Patterns cv;
Btn btns[] = { { STEPUP, 12 },{ STEPDN, 11 },{ ENCODER, 10 } };
Encoder myEnc(ENCCLKPIN, ENCDATAPIN);
ClockHandler clock(minBPM, maxBPM);
DisplayHandler dispHandler;

void initSequence(seqType seqtype, int seqNum, seqInitType initType, unsigned int numSteps = 8) {
	cv.seq[seqNum].type = seqtype;
	numSteps = (numSteps == 0 || numSteps > 8 ? 8 : numSteps);
	cv.seq[seqNum].steps = numSteps;
	for (int s = 0; s < 8; s++) {
		cv.seq[seqNum].Steps[s].volts = (initType == INITBLANK ? 2.5 : ((double)rand() / (double)RAND_MAX) * 5);
		cv.seq[seqNum].Steps[s].rand_amt = (initType == INITBLANK ? 0 : round(((double)rand() / (double)RAND_MAX) * 10));
	}
}

void setup() {
	pinMode(LED, OUTPUT);

	analogWriteResolution(12);    // set resolution of DAC pin for outputting variable voltages
	//	initialise all momentary buttons as Input pullup
	for (int b = 0; b < 3; b++) {
		pinMode(btns[b].pin, INPUT_PULLUP);
	}

	//  Set up sequence
	for (int p = 1; p <= 8; p++) {
		initSequence(CV, p, INITRAND);
	}

	// Setup OLED
	dispHandler.init();

	// initialiase encoder
	oldEncPos = round(myEnc.read() / 4);

	Serial.println(sizeof(cv));
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
	unsigned int timeStep = 1000 / (((float)bpm / 60) * 2);		// get length of step based on bpm
	if (timeCounter >= timeStep && (!clock.hasSignal() || millis() - clock.clockHighTime < 10)) {

		//	increment sequence step
		seqStep += 1;
		if (seqStep == cv.seq[sequenceA].steps) {
			seqStep = 0;
		}

		// calculate possible ranges of randomness to ensure we don't try and set a random value out of permitted range
		float randLower = cv.seq[sequenceA].Steps[seqStep].volts - ((double)cv.seq[sequenceA].Steps[seqStep].rand_amt / 2);
		float randUpper = cv.seq[sequenceA].Steps[seqStep].volts + ((double)cv.seq[sequenceA].Steps[seqStep].rand_amt / 2);
		randAmt = constrain(randLower + ((double)rand() / (double)RAND_MAX) * (randUpper - randLower), 0, voltsMax);

		if (DEBUGSTEPS) {
			Serial.print("S: "); Serial.println(seqStep);
			Serial.print("V: "); Serial.print(cv.seq[sequenceA].Steps[seqStep].volts); Serial.print(" R: "); Serial.println(cv.seq[sequenceA].Steps[seqStep].rand_amt);
			Serial.print("L: "); Serial.print(randLower); Serial.print(" U: "); Serial.println(randUpper);
			Serial.print("result: "); Serial.println(randAmt);
		}

		setCV(randAmt);
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

			if (editMode == STEPV && editStep >= 0) {
				cv.seq[sequenceA].Steps[editStep].volts += newEncPos > oldEncPos ? 0.10 : -0.10;
				cv.seq[sequenceA].Steps[editStep].volts = constrain(cv.seq[sequenceA].Steps[editStep].volts, 0, 5);
				Serial.print("volts: ");  Serial.print(cv.seq[sequenceA].Steps[editStep].volts);
			}
			if (editMode == STEPR && editStep >= 0) {
				cv.seq[sequenceA].Steps[editStep].rand_amt += newEncPos > oldEncPos ? 1 : -1;
				cv.seq[sequenceA].Steps[editStep].rand_amt = constrain(cv.seq[sequenceA].Steps[editStep].rand_amt, 0, 10);
				Serial.print("rand: ");  Serial.print(cv.seq[sequenceA].Steps[editStep].rand_amt);
			}
			
			//	sequence select mode
			if (editMode == PATTERN && editStep == -1) {
				sequenceA += newEncPos > oldEncPos ? 1 : -1;
				sequenceA = sequenceA < 1 ? 8 : (sequenceA > 8 ? 1 : sequenceA);
				Serial.print("pattern: ");  Serial.print(sequenceA);
			}
			if (editMode == SEQS && editStep == -1) {
				numSeqA += newEncPos > oldEncPos ? 1 : -1;
				numSeqA = constrain(numSeqA, 1,  8);
				Serial.print("seqs: ");  Serial.print(numSeqA);
			}

			if (editMode == SEQMODE && editStep == -1) {
				modeSeqA = !modeSeqA;
				Serial.print("mode: ");  Serial.print(modeSeqA);
			}
		}

		oldEncPos = newEncPos;
		Serial.print("  Encoder: ");  Serial.println(newEncPos);
		lastEncoder = millis();
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

					// check editing mode is valid for selected step type
					checkEditState();

					Serial.println("Param up/dn");
				}
				if (btns[b].name == ENCODER) {
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
					lastEncoder = millis();
					Serial.println("Encoder button");
				}

			}
			btns[b].pressed = 1;
			btns[b].lastPressed = millis();
		}
	}

	if (dispHandler.displayRefresh > 0 && millis() > 1000) {
		dispHandler.updateDisplay();
	}

}


void setCV(float setVolt) {
	//  DAC buffer takes values of 0 to 4095 relating to 0v to 3.3v
	//  setVolt will be in range 0 - voltsMax (5 unless trying to do pitch which might need negative)
	float dacVolt = setVolt / voltsMax * 4095;
	analogWrite(DACPIN, (int)dacVolt);

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