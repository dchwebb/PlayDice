//#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Encoder.h>
#include "Adafruit_SSD1306.h"
#include "ClockHandler.h"
#include "DisplayHandler.h"
#include "SetupFunctions.h"
#include "Settings.h"
//#include <array>
//#include <algorithm>


//	declare variables
uint16_t tempoPot = 512, oldTempoPot = 0;		// Reading from tempo potentiometer for setting bpm
uint16_t bpm = 120;				// beats per minute of sequence (assume sequence runs in eighth notes for now)
uint16_t minBPM = 35;			// minimum BPM allowed for internal/external clock
uint16_t maxBPM = 300;			// maximum BPM allowed for internal/external clock
elapsedMillis timeCounter = 0;  // millisecond counter to check if next sequence step is due
elapsedMillis debugCounter = 0;	// used to show debug data only every couple of ms
uint32_t guessNextStep;			// guesstimate of when next step will fall - to avoid display firing at wrong time
uint32_t lastEditing = 0;		// ms counter to show detailed edit parameters while editing or just after
boolean saveRequired;			// set to true after editing a parameter needing a save (saves batched to avoid too many writes)
boolean autoSave = 1;			// set to true if autosave enabled
float cvRandVal = 0;			// Voltage of current step with randomisation applied
boolean gateRandVal;			// 1 or 0 according to whether gate is high or low after randomisation
boolean triggerMode = 0;		// Gate sequencer outputs triggers rather than gates
uint32_t lastGate;				// Time gate was last set to high for use with trigger mode
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
boolean pause;					// if true pause sequencers
boolean actionStutter;			// Stutter triggered by action button
uint8_t stutterStep;			// When stutter is triggered by action button store stutter step number based on current clock speed 
uint8_t actionStutterNo = 8;	// Number of stutter steps when triggered by action button
float lfoX = 1, lfoY = 0;		// LFO parameters for quick Minsky approximation
float lfoSpeed;					// lfoSpeed calculated from tempo pot
float oldLfoSpeed;				// lfoSpeed calculated from tempo pot
uint8_t lfoJitter;				// because the analog pot is more sensitive at the bottom of its range adjust the threshold before detecting a pot turn
boolean pitchMode;				// set to true if CV lane displays and quantises to pitches
uint8_t quantRoot;				// if quantising in pitchmode sets root note
uint8_t quantScale;				// if quantising in pitchmode sets scale
uint8_t oldRoot = -1;				// previous root note to check if we need to rebuild quantise table
uint8_t oldScale;				// previous scale
elapsedMillis lfoCounter = 0;	// millisecond counter to check if next lfo calculation is due
uint8_t submenuSize;			// number of items in array used to pick from submenu items
uint8_t submenuVal;				// currently selected submenu item
String clockDiv = "";			// shows whether a clock divider is in place in the setup menu (clocked input with multiplier/divider provided by tempo pot)
int8_t cvOffset;				// adds an offset to the CV > DAC conversion to account for component tolerance etc
actionOpts actionCVType = ACTSTUTTER;
actionOpts actionBtnType = ACTPAUSE;

struct CvPatterns cv;
struct GatePatterns gate;
struct QuantiseRange quantiseRange[12];

Btn btns[] = { { STEPDN, 22 },{ STEPUP, 12 },{ ENCODER, 15 },{ CHANNEL, 19 },{ ACTIONBTN, 20 },{ ACTIONCV, 21 } };		// numbers refer to Teensy digital pin numbers

ClockHandler clock(minBPM, maxBPM);
DisplayHandler dispHandler;
SetupMenu setupMenu;
Encoder myEnc(ENCCLKPIN, ENCDATAPIN);



void setup() {

	pinMode(LED, OUTPUT);
	pinMode(GATEOUT, OUTPUT);
	pinMode(CLOCKPIN, INPUT_PULLUP);

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
			initCvSequence(p, INITRAND, 8);
			srand(micros());
			initGateSequence(p, INITRAND, 8);
		}
	}
	cvSeqNo = cvLoopFirst;
	gateSeqNo = gateLoopFirst;
	makeQuantiseArray();

	// initialise encoder
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
			lfoSpeed = (float)(pow((float)tempoPot, (float)0.72)) / 50000;
			if (lfoSpeed < 0.0001) {
				lfoSpeed = 0.0001;
			}

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

#if DEBUGCLOCK
	if (debugCounter > 5) {
		debugCounter = 0;
		clock.printDebug();
	}
#endif

	tempoPot = analogRead(TEMPOPIN);		//  read value of potentiometer to set speed

	// work out whether to get bpm from tempo potentiometer or clock signal (checking that we have recieved a recent clock signal)
	if (clockBPM >= minBPM && clockBPM < maxBPM && clock.hasSignal()) {
		bpm = clockBPM;
		// basic clock divider allowing tempo to be halved or doubled depending on position of tempo pot
		if (tempoPot < 205) {
			bpm = clockBPM / 4;
			clockDiv = "/4";
		} else if (tempoPot < 410) {
			bpm = clockBPM / 2;
			clockDiv = "/2";
		} else if (tempoPot > 820) {
			bpm = clockBPM * 4;
			clockDiv = "x4";
		} else if (tempoPot > 615) {
			bpm = clockBPM * 2;
			clockDiv = "x2";
		} else {
			clockDiv = "";
		}
		//Serial.print("cl bpm: ");  Serial.print(clockBPM); Serial.print(" tp: ");  Serial.print(tempoPot); Serial.print(" bpm: ");  Serial.println(bpm);
	}
	else {
		bpm = map(tempoPot, 0, 1023, minBPM, maxBPM);        // map(value, fromLow, fromHigh, toLow, toHigh)
		clockDiv = "";
	}

	//	check if the sequence counter is ready to advance to the next step. Also if using external clock wait for pulse
	uint16_t timeStep = 1000 / (((float)bpm / 60) * 2);		// get length of step based on bpm

	boolean newStep = (timeCounter >= timeStep && (!clock.hasSignal() || millis() - clock.clockHighTime < 10));

	//	When the clock is slow relative to the speed of the tempo (usually at 4x clock speed) there won't be a clock tick for every beat
	if (!newStep && timeCounter >= timeStep && clock.clockInterval + 10 >= timeCounter + timeStep && millis() - clock.clockHighTime > clock.clockInterval / 2) {
#if DEBUGSTEP
Serial.print("m-ch: ");  Serial.print(millis() - clock.clockHighTime); Serial.print(" ci int: ");  Serial.print(clock.clockInterval); Serial.print(" timeStep: ");  Serial.print(timeStep); Serial.print(" tc: ");  Serial.print(timeCounter); Serial.print(" bpm: ");  Serial.println(bpm);
#endif
		newStep = 1;
	}	

	boolean newGateStutter = 0;
	boolean newCVStutter = 0;
	if (gateStutterStep > 0 && gateStutterStep < gate.seq[gateSeqNo].Steps[gateStep].stutter && !pause) {
		if (timeCounter >= gateStutterStep * (timeStep / gate.seq[gateSeqNo].Steps[gateStep].stutter)) {
#if DEBUGSTEP
	Serial.print("new gate stutter step: ");  Serial.print(gateStutterStep);  Serial.print(" ms "); Serial.println(millis());
#endif
			newGateStutter = 1;
		}
	}
	if (cvStutterStep > 0 && cvStutterStep < cv.seq[cvSeqNo].Steps[cvStep].stutter && !pause) {
		if (timeCounter >= cvStutterStep * (timeStep / cv.seq[cvSeqNo].Steps[cvStep].stutter)) {
#if DEBUGSTEP
			Serial.print("new cv stutter step: "); Serial.println(millis());
#endif
			newCVStutter = 1;
		}
	}

	//	if action button triggers a stutter check which step to activate
	if (actionStutter) {
		//if (DEBUGBTNS) { Serial.print("actionStutter: "); Serial.println(actionStutter); }
		//	divide current step length by stutter count - for now actionStutterNo
		if (stutterStep == 0) {
			stutterStep = (timeCounter / (timeStep / actionStutterNo)) + 1;
			Serial.print("tc: "); Serial.print(timeCounter);  Serial.print(" ts ");  Serial.println(timeStep);
		}
		if (timeCounter >= stutterStep * (timeStep / actionStutterNo) && stutterStep < actionStutterNo && !pause) {
#if DEBUGSTEP
			Serial.print("action stutter step: "); Serial.print(stutterStep);  Serial.print(" stutter no: "); Serial.print(actionStutterNo);  Serial.print("   ms ");  Serial.println(millis());
#endif
			newGateStutter = 1;
			newCVStutter = 1;
		}
	}
	else {
		stutterStep = 0;
	}


	if (!pause && (newStep || newGateStutter || newCVStutter)) {
		//	increment sequence step and reinitialise stutter steps
		if (newStep) {
			cvStep += 1;
			if (cvStep >= cv.seq[cvSeqNo].steps) {
				cvStep = 0;
				if (!checkEditing() && cvLoopLast > cvLoopFirst) {
					cvSeqNo = cvSeqNo++ >= cvLoopLast ? cvLoopFirst : cvSeqNo;
					makeQuantiseArray();
#if DEBUGSTEP
					Serial.print("CV seq: "); Serial.println(cvSeqNo);
#endif
				}
			}
			gateStep += 1;
			if (gateStep >= gate.seq[gateSeqNo].steps) {
				gateStep = 0;
				if (!checkEditing() && gateLoopLast > gateLoopFirst) {
					gateSeqNo = gateSeqNo++ >= gateLoopLast ? gateLoopFirst : gateSeqNo;
#if DEBUGSTEP
					Serial.print("Gate seq: "); Serial.println(gateSeqNo);
#endif
				}
			}
			cvStutterStep = 0;
			gateStutterStep = 0;
			stutterStep = 0;
#if DEBUGSTEP
			Serial.print("new step: "); Serial.print(cvStep); Serial.print(" millis: "); Serial.println(millis());
#endif

		}

		//	guess the next step or stutter time to estimate if we have time to do a refresh
		guessNextStep = millis() + min(min((gate.seq[gateSeqNo].Steps[gateStep].stutter ? (timeStep / gate.seq[gateSeqNo].Steps[gateStep].stutter) : timeStep),
			(cv.seq[cvSeqNo].Steps[cvStep].stutter ? (timeStep / cv.seq[cvSeqNo].Steps[cvStep].stutter) : timeStep)),
			(actionStutter ? (timeStep / actionStutterNo) : timeStep)
		);

#if DEBUGSTEP
		Serial.print("guess next step: "); Serial.println(guessNextStep);
#endif

		// CV sequence: calculate possible ranges of randomness to ensure we don't try and set a random value out of permitted range
		if (newStep || newCVStutter || actionStutter) {
			if (cv.seq[cvSeqNo].Steps[cvStep].stutter > 0 || actionStutter) {
				if (actionStutter) {
					cvStutterStep = stutterStep;
				}
				cvStutterStep += 1;
			}
			if (cv.seq[cvSeqNo].Steps[cvStep].rand_amt) {
				float randLower = getRandLimit(cv.seq[cvSeqNo].Steps[cvStep], LOWER);
				float randUpper = getRandLimit(cv.seq[cvSeqNo].Steps[cvStep], UPPER);
				cvRandVal = constrain(randLower + (getRand() * (randUpper - randLower)), 0, 5);
#if DEBUGRAND
				Serial.print("CV  S: "); Serial.print(cvStep);	Serial.print(" V: "); Serial.print(cv.seq[cvSeqNo].Steps[cvStep].volts); Serial.print(" Rnd: "); Serial.println(cv.seq[cvSeqNo].Steps[cvStep].rand_amt);
				Serial.print("    Lwr: "); Serial.print(randLower); Serial.print(" Upr: "); Serial.print(randUpper); Serial.print(" Result: "); Serial.println(cvRandVal);
#endif

			}
			else {
				cvRandVal = cv.seq[cvSeqNo].Steps[cvStep].volts;
			}
			setCV(cvRandVal);
		}

		// Gate sequence: calculate probability of gate being high or low. Eg rand_amt = 9 means there is a 90% chance that the value will be randomised
		if (newStep || newGateStutter || actionStutter) {

			if (gate.seq[gateSeqNo].Steps[gateStep].stutter > 0 || actionStutter) {
				if (actionStutter) {
					gateStutterStep = stutterStep;
					
				}
				gateStutterStep += 1;
				gateRandVal = ((gateStutterStep + (gate.seq[gateSeqNo].Steps[gateStep].on ? 0 : 1)) % 2 > 0);
#if DEBUGSTEP
				Serial.print("Step: "); Serial.print(gateStep); Serial.print(" grv: "); Serial.print(gateRandVal); Serial.print(" gss: "); Serial.println(gateStutterStep);
#endif
			}
			else {
				if (gate.seq[gateSeqNo].Steps[gateStep].rand_amt) {
					uint8_t rndXTen = getRand() * 10;
					float r = getRand();
					gateRandVal = (gate.seq[gateSeqNo].Steps[gateStep].rand_amt > rndXTen && r < 0.5) ? !gate.seq[gateSeqNo].Steps[gateStep].on : gate.seq[gateSeqNo].Steps[gateStep].on;

#if DEBUGRAND
					Serial.print("GT on: "); Serial.print(gate.seq[gateSeqNo].Steps[gateStep].on); Serial.print(" prb: "); Serial.print(gate.seq[gateSeqNo].Steps[gateStep].rand_amt); Serial.print(" > rand: "); Serial.print(rndXTen);
					Serial.print(" Gate: "); Serial.print(r); Serial.print(" changed: "); Serial.println(gate.seq[gateSeqNo].Steps[gateStep].on != gateRandVal);
#endif
				}
				else {
					gateRandVal = gate.seq[gateSeqNo].Steps[gateStep].on;
				}

			}
			digitalWrite(GATEOUT, gateRandVal);
			if (triggerMode && gateRandVal) {
				lastGate = millis();
#if DEBUGSTEP
				Serial.println("Gate off - trigger mode"); 
#endif
			}

		}

		if (newStep || gateStutterStep > 0 || cvStutterStep > 0 || actionStutter) {
			stutterStep += 1;
		}

		//  reset step info and flash LED
		if (newStep) {
			//digitalWrite(LED, cvStep % 2 == 0 ? HIGH : LOW);
			timeCounter = 0;
			newStep = 0;
		}
	}
	else if (triggerMode && gateRandVal && lastGate > 0 && lastGate < millis() - 10) {
		digitalWrite(GATEOUT, 0);
		lastGate = 0;
	}


	// Handle Encoder turn - alter parameter depending on edit mode
	long newEncPos = myEnc.read();
	if (newEncPos != oldEncPos) {

		// check editing mode is valid for selected step type
		checkEditState();
		if (round(newEncPos / 4) != round(oldEncPos / 4)) {
			boolean upOrDown = newEncPos > oldEncPos;

			if (editMode == SETUP || editMode == SUBMENU) {
				setupMenu.menuPicker(upOrDown ? ENCUP : ENCDN);
			}
			else {

				// change parameter
				if (editStep >= 0) {

					if (activeSeq == SEQCV) {
						CvStep *s = &cv.seq[cvSeqNo].Steps[editStep];
						if (editMode == STEPV) {
							if (pitchMode) {
								float n = s->volts + (upOrDown ? 0.08333 : -0.08333);
								Serial.print("n: "); Serial.print(n, 3); Serial.print("qn: "); Serial.print(quantiseVolts(n), 3); Serial.print(" sn: "); Serial.println(quantiseVolts(s->volts), 3);
								while (round(quantiseVolts(n) * 100) == round(quantiseVolts(s->volts) * 100)) {
									n += (upOrDown ? 0.08333 : -0.08333);
									Serial.print("aq: "); Serial.println(n, 3);
								}
								s->volts = quantiseVolts(n);
								//s->volts = (s->volts + (upOrDown ? 0.08333 : -0.08333));//quantiseVolts
							}
							else {
								s->volts += upOrDown ? 0.10 : -0.10;
							}
							s->volts = constrain(s->volts, 0, 5);
							Serial.print("Edit volts: "); Serial.println(s->volts);
						}
						if (editMode == STEPR && (upOrDown || s->rand_amt > 0) && (!upOrDown || s->rand_amt < 10)) {
							s->rand_amt += upOrDown ? 1 : -1;
							Serial.print("Edit rand: "); Serial.println(s->rand_amt);
						}
						if (editMode == STUTTER && (upOrDown || s->stutter > 0) && (!upOrDown || s->stutter < 8)) {
							s->stutter += upOrDown ? (s->stutter == 0 ? 2 : 1) : (s->stutter == 2 ? -2 : -1);
							Serial.print("Edit stutter: "); Serial.println(s->stutter);
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
						if (activeSeq == SEQCV) {
							cvSeqNo += upOrDown ? (cvSeqNo < 7 ? 1 : 0) : cvSeqNo > 0 ? -1 : 0;// because we are using an unsigned int -1 goes to 255
							if (cvLoopFirst == cvLoopLast) {
								cvLoopFirst = cvLoopLast = cvSeqNo;
							}
							makeQuantiseArray();
						}
						else {
							gateSeqNo += upOrDown ? (gateSeqNo < 7 ? 1 : 0) : gateSeqNo > 0 ? -1 : 0;
							if (gateLoopFirst == gateLoopLast) {
								gateLoopFirst = gateSeqNo;
								gateLoopLast = gateSeqNo;
							}
						}
#if DEBUGBTNS
						Serial.print("cv pat: "); Serial.print(cvSeqNo); Serial.print(" gate: "); Serial.print(gateSeqNo); 
#endif
					}

					if (editMode == LOOPFIRST) {
						uint8_t * loopF = activeSeq == SEQCV ? &cvLoopFirst : &gateLoopFirst;
						uint8_t * loopL = activeSeq == SEQCV ? &cvLoopLast : &gateLoopLast;
						*loopF += upOrDown ? 1 : -1;
						*loopF = *loopF == 255 ? 7 : (*loopF > 7 ? 0 : *loopF);		// because we are using an unsigned int -1 goes to 255
						*loopL = constrain(*loopL, *loopF, 7);
#if DEBUGBTNS
						Serial.print("First loop: ");  Serial.print(*loopF);
#endif
					}

					if (editMode == LOOPLAST) {
						uint8_t * loopF = activeSeq == SEQCV ? &cvLoopFirst : &gateLoopFirst;
						uint8_t * loopL = activeSeq == SEQCV ? &cvLoopLast : &gateLoopLast;
						*loopL += upOrDown ? 1 : -1;
						*loopL = *loopL == 255 ? *loopF : (*loopL > 7 ? 7 : *loopL);		// because we are using an unsigned int -1 goes to 255
						*loopL = constrain(*loopL, *loopF, 7);
#if DEBUGBTNS
						Serial.print("Last loop: ");  Serial.print(*loopL);
#endif
					}

					//	Sequence mode (gate/trigger or CV/Pitch)
					if (editMode == SEQMODE) {
						if (activeSeq == SEQCV) {
							cv.seq[cvSeqNo].mode = !cv.seq[cvSeqNo].mode;
						}
						else {
							gate.seq[gateSeqNo].mode = !gate.seq[gateSeqNo].mode;
						}
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
					if (editMode == SEQOPT) {
						submenuVal += (upOrDown ? 1 : -1);
						submenuVal = submenuVal == 255 ? initSeqSize - 1 : submenuVal > initSeqSize - 1 ? 0 : submenuVal;
					} 

					//	Pitch mode root and scale selection
					if (editMode == SEQROOT) {
						cv.seq[cvSeqNo].root = AddNLoop(cv.seq[cvSeqNo].root, upOrDown, 11);
						makeQuantiseArray();
					}
					if (editMode == SEQSCALE) {
						cv.seq[cvSeqNo].scale = AddNLoop(cv.seq[cvSeqNo].scale, upOrDown, 2);
					}

				}
				lastEditing = millis();
				saveRequired = 1;
			}
#if DEBUGBTNS
			Serial.print("  Encoder: ");  Serial.println(newEncPos);
#endif
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
#if DEBUGBTNS
					Serial.println("Btn: Channel");
#endif
					if (editMode == SETUP) {
						normalMode();
					}
					else {
						activeSeq = (activeSeq == SEQGATE ? SEQCV : SEQGATE);
						lastEditing = 0;
					}
				}
			}

			if (btns[b].released && (btns[b].name == ACTIONBTN || btns[b].name == ACTIONCV)) {
				btns[b].released = 0;
				actionStutter = 0;
#if DEBUGBTNS
				Serial.println("Stutter off");
#endif
			}
		}
		else {
			//  check if button has been pressed (previous state off and over x milliseconds since last on)
			if (btns[b].pressed == 0 && millis() - btns[b].lastPressed > 10) {

				if (btns[b].name != ACTIONBTN && btns[b].name != ACTIONCV && (editMode == SETUP || editMode == SUBMENU)) {
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

#if DEBUGBTNS
							if (btns[STEPUP].pressed) Serial.println("Btn: Step up");
							if (btns[STEPDN].pressed) Serial.println("Btn: Step dn");
#endif
					}

					if (btns[b].name == ACTIONBTN || btns[b].name == ACTIONCV) {
						actionOpts actionType = btns[b].name == ACTIONBTN ? actionBtnType : actionCVType;
#if DEBUGBTNS
						Serial.print("Btn: Action; type: "); Serial.println(actionType);
#endif

						switch (actionType) {
						case ACTSTUTTER:
							actionStutter = 1;
							break;
						case ACTRESTART:
							cvStep = 0;
							gateStep = 0;
							break;
						case ACTPAUSE:
							pause = !pause;
							// ensure gate is off when pausing
							if (pause) {
								digitalWrite(GATEOUT, 0);
							}
							break;
						}
					}


					if (btns[b].name == ENCODER) {
#if DEBUGBTNS
						Serial.println("Btn: Encoder");
#endif
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
								if (submenuVal == 0) {
									editMode = (activeSeq == SEQCV && cv.seq[cvSeqNo].mode == PITCH) ? SEQROOT : SEQMODE;
								} else  {		// "None", "All", "Vals", "Blank"
									activeSeq == SEQCV ? initCvSequence(cvSeqNo, (seqInitType)submenuVal, cv.seq[cvSeqNo].steps) : initGateSequence(gateSeqNo, (seqInitType)submenuVal, gate.seq[gateSeqNo].steps);
								}
								break;
							case SEQROOT:
								editMode = SEQSCALE;
								break;
							case SEQSCALE:
								editMode = SEQMODE;
								break;
							case SETUP:
								break;
							case SUBMENU:
								break;
							case LFO:
								break;
							case NOISE:
								break;
							}
						}
						else {
							// if not currently editing initialise to first step of editing menu
							editMode = editStep == -1 ? SEQMODE : STEPV;
							submenuVal = 0;
						}

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
#if DEBUGBTNS
		Serial.println("Setup");
#endif
		
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








double getRand() {
	return (double)rand() / (double)RAND_MAX;
}

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

void setCV(float setVolt) {
	//  DAC buffer takes values of 0 to 4095 relating to 0v to 3.3v
	//  setVolt will be in range 0 - voltsMax (5 unless trying to do pitch which might need negative)
	if (pitchMode) {
		setVolt = quantiseVolts(setVolt);
	}
	float dacVolt = (setVolt / 5 * 4095) + cvOffset;
	analogWrite(DACPIN, (int)dacVolt);
}



float quantiseVolts(float v) {
	
#if DEBUGQUANT
	Serial.print("v in: "); Serial.print(v, 3);
#endif

	if (cv.seq[cvSeqNo].scale == 0) {		// chromatic
		v = (float)round(v * 12) / 12;
	}
	else {
		float v1 = v - int(v);
		for (int8_t x = 0; x < 12; x++) {
			if (v1 <= quantiseRange[x].to) {
				v = int(v) + quantiseRange[x].target;
				break;
			}
		}
	}

#if DEBUGQUANT
	Serial.print("  v out: "); Serial.print(v, 3);Serial.print("  "); Serial.println(dispHandler.pitchFromVolt(v));
#endif

	return v;
}
//
//uint8_t AddAndLoop(uint8_t x, boolean add, uint8_t max) {
//	// adds or subtracts one from a number, looping back to zero if > max or to max if < 0
//	return (x == max && add) ? 0 : (x == 0 && !add) ? max : add ? x + 1 : x - 1;
//}


boolean checkEditing() {
	// check if recent encoder activity
	return (editMode != PATTERN && lastEditing > 1 && millis() - lastEditing < 5000);
}

void checkEditState() {
	// check editing mode is valid for selected step type
	if (editMode != SETUP && editMode != SUBMENU) {
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
	}
	else {
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

void makeQuantiseArray() {
	//	makes an array of each scale note voltage with the upper limit of CV that will be quantised to that note
#if DEBUGQUANT
	if (millis() < 1000) {
		delay(500);
	}
#endif

	/*
	0.000	C		0.000	C		0.167	D		0.083	C#
	0.083	C#		0.167	D		0.333	E		0.167	D
	0.167	D		0.333	E		0.500	F#		0.333	E
	0.250	D#		0.417	F		0.583	G		0.500	F#
	0.333	E		0.583	G		0.750	A		0.583	G
	0.417	F		0.750	A		0.917	B		0.750	A
	0.500	F#		0.917	B		1.083	C#		0.917	B
	0.583	G		1.000	C		1.167	D		1.167	C#
	0.667	G#
	0.750	A
	0.833	A#
	0.917	B
	1.000	C
	*/

	// no need to generate quantise table if not in pitched mode, chromatic scale or still using previous scale
	if (cv.seq[cvSeqNo].mode != PITCH || cv.seq[cvSeqNo].scale == 0 || (cv.seq[cvSeqNo].root == oldRoot && cv.seq[cvSeqNo].scale == oldScale)) {
		return;
	}

	uint8_t lookupPos = 0;
	uint8_t s = 0;
	float targCurr, targPrev, toPrev = 0;
	for (uint8_t n = 0; n < 25; n++) {
		if (scaleNotes[cv.seq[cvSeqNo].scale][n % 12] == 1) {

			targCurr = 0.083333 * (n + cv.seq[cvSeqNo].root);
			if (lookupPos > 0) {
				// get upper range of previous scale note by averaging difference
				toPrev = targPrev + ((targCurr - targPrev) / 2);
			}
#if DEBUGQUANT
			//Serial.print(n); Serial.print(" toPrev: "); Serial.print(toPrev); Serial.print("  targCurr: "); Serial.print(targCurr); Serial.print(" targPrev: "); Serial.println(targPrev);
#endif
			// once we have got beyond the first octave rewrite sequence so that it is ordered but starting from 0 volts
			if (toPrev > 1) {
				quantiseRange[s].target = targPrev - (float)1;
				quantiseRange[s].to = toPrev - (float)1;
				s += 1;
			}
			targPrev = targCurr;
			lookupPos += 1;
		}
	}

	oldRoot = cv.seq[cvSeqNo].root;
	oldScale = cv.seq[cvSeqNo].scale;

#if DEBUGQUANT
	Serial.print("Quantise scale: "); Serial.println(scales[cv.seq[cvSeqNo].scale]);
	Serial.print("Root adjust: "); Serial.println(pitches[cv.seq[cvSeqNo].root]);
	for (uint8_t n = 0; n < 12; n++) {
		Serial.print(n); Serial.print(" target: "); Serial.print(quantiseRange[n].target, 3); Serial.print("  to: "); Serial.println(quantiseRange[n].to, 3);
	}
#endif

}