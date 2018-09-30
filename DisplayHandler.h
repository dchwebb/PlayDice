#pragma once
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"
#include "Settings.h"
#include "SetupFunctions.h"
#include "WString.h"
#include <array>

extern const boolean DEBUGFRAME;

extern uint16_t bpm;
extern int8_t cvStep;
extern int8_t gateStep;
extern int8_t editStep;
extern editType editMode;
extern float cvRandVal;
extern boolean gateRandVal;
extern uint8_t cvSeqNo;
extern uint8_t gateSeqNo;
extern seqType activeSeq;
extern CvPatterns cv;
extern GatePatterns gate;
extern uint8_t cvLoopFirst;		// first sequence in loop
extern uint8_t cvLoopLast;		// last sequence in loop
extern uint8_t gateLoopFirst;	// first sequence in loop
extern uint8_t gateLoopLast;	// last sequence in loop
extern boolean pitchMode;		// set to true if CV mode displays as pitches
extern uint8_t quantRoot;		// if quantising in pitchmode sets root note
extern uint8_t quantScale;		// if quantising in pitchmode sets scale
extern SetupMenu setupMenu;
extern float getRandLimit(CvStep s, rndType getUpper);
extern double getRand();
extern boolean checkEditing();
extern ClockHandler clock;
extern const String pitches[];
extern const String *submenuArray;
extern uint8_t submenuSize;			// number of items in array used to pick from submenu items
extern uint8_t submenuVal;				// currently selected submenu item

class DisplayHandler {
public:
	DisplayHandler();
	void updateDisplay();
	void init();
	int cvVertPos(float voltage);
	void drawDottedVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
	void drawParam(String s, String v, uint8_t x, uint8_t y, uint8_t w, boolean selected, uint8_t highlightX, uint8_t highlightW);
	void drawParam(String s, String v, uint8_t x, uint8_t y, uint8_t w, boolean selected);
	void displayLanes();
	void displayLFO();
	void displaySetup();
	String pitchFromVolt(float v);
	Adafruit_SSD1306 display;
private:
	long clockSignal;
	int32_t frameStart;
};

//	Putting the constructor here with display class initialised after colon ensures that correct constructor gets called and does not blank settings
DisplayHandler::DisplayHandler() :
	display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS) {
}

// carry out the screen refresh building the various UI elements
void DisplayHandler::updateDisplay() {
	if (DEBUGFRAME) {
		frameStart = micros();
	}
	display.clearDisplay();
	display.setTextSize(1);

	if (editMode == LFO || editMode == NOISE) {
		displayLFO();
	}
	else if (editMode == SETUP || editMode == SUBMENU) {
		displaySetup();
	} 
	else {
		displayLanes();
	}

	if (display.display(editMode == LFO || editMode == NOISE) && DEBUGFRAME) {
		int32_t m = micros();
		Serial.print("Frame start: "); Serial.print(frameStart); Serial.print(" end: "); Serial.print(m); Serial.print(" time: "); Serial.println(m - frameStart);
	}
}

//	Display static lfo screen
void DisplayHandler::displayLFO() {
	//display.drawRect(0, 0, 128, 64, WHITE);
	float e = 0.3;
	float x = 1, y = 0, oldY = 0;
	for (int i = 0; i < 110; ++i)
	{
		x -= e * y;
		y += e * x;
		if (editMode == LFO) {
			display.drawPixel(i + 10, 7 + round(8 * (y + 1)), WHITE);	// draw sine wave
		}
		else {
			display.drawPixel(i + 10, 7 + round(16 * getRand()), WHITE);	// draw noise wave
		}
		display.drawPixel(i + 10, y > 0 ? 55 : 40, WHITE);	// draw square wave

		// draw vertical lines on square wave if changing from high to low
		if ((oldY > 0) != (y > 0)) {
			display.drawFastVLine(i + 10, 40, 15, WHITE);
		}
		oldY = y;
	}
	display.drawRect(0, 0, 128, 64, WHITE);
	display.drawFastHLine(0, 31, 128, WHITE);
}

//	Display setup menu
void DisplayHandler::displaySetup() {
	display.drawRect(0, 0, 128, 64, WHITE);
	display.setTextSize(1);
	display.setCursor(5, 4);
	display.print("Setup");
	display.drawFastHLine(0, 15, 128, WHITE);
	display.setCursor(65, 4);
	display.print("BPM");
	display.setCursor(85, 4);
	display.print(bpm);

	if (clock.hasSignal()) {
		display.setCursor(110, 4);
		display.print("C");
	}

	if (editMode == SUBMENU) {

		uint8_t menuStart = round(submenuVal / 4) * 4;
		for (uint8_t m = 0; m < 4; m++) {
			// check if there is a menu item to draw
			if (menuStart + m < submenuSize) {
				display.setCursor(10, 20 + (m * 10));
				String s = submenuArray[menuStart + m];
				display.print(s);

				//Serial.print("name: "); Serial.print(s); Serial.print(" len: "); Serial.print(s.length());
				if (menuStart + m == submenuVal) {
					display.fillRect(8, 19 + (m * 10), s.length() * 8, 10, INVERSE);
				}
			}
		}
		// draw up/down arrows if there are pages of menu items before/after current page
		if (submenuVal > 3) {
			display.setCursor(120, 20);
			display.write(24);		// writes an arrow from the Adafruit library
		}
		if (menuStart + 4 < submenuSize) {
			display.setCursor(120, 50);
			display.write(25);		// writes an arrow from the Adafruit library
		}

	}
	else {

		// can fit four items in menu so check if scrolling required
		uint8_t menuSelected = 0;
		for (uint8_t m = 0; m < setupMenu.size(); m++) {
			if (setupMenu.menuSelected(m)) {
				menuSelected = m;
				break;
			}
		}

		uint8_t menuStart = round(menuSelected / 4) * 4;
		for (uint8_t m = 0; m < 4; m++) {
			// check if there is a menu item to draw
			if (menuStart + m < setupMenu.size()) {
				display.setCursor(5, 20 + (m * 10));
				String v = setupMenu.menuVal(menuStart + m);
				String s = setupMenu.menuName(menuStart + m) + (String)(v != "" ? ":" + v : "");
				display.print(s);

				//Serial.print("name: "); Serial.print(s); Serial.print(" len: "); Serial.print(s.length());
				if (setupMenu.menuSelected(menuStart + m)) {
					display.fillRect(3, 19 + (m * 10), s.length() * 7, 10, INVERSE);
				}
			}
		}
		// draw up/down arrows if there are pages of menu items before/after current page
		if (menuSelected > 3) {
			display.setCursor(120, 20);
			display.write(24);		// writes an arrow from the Adafruit library
		}
		if (menuStart + 4 < setupMenu.size()) {
			display.setCursor(120, 50);
			display.write(25);		// writes an arrow from the Adafruit library
		}
	}

}

//	Display CV and Gate Lanes
void DisplayHandler::displayLanes() {
	boolean editing = checkEditing();		// set to true if currently editing to show detailed parameters

	//	Write the sequence number for CV and gate sequence
	if (!editing || activeSeq == SEQCV) {
		display.setCursor(0, 0);
		display.print("cv");
	}
	if (!editing || activeSeq == SEQGATE) {
		display.setCursor(0, 39);
		display.print("Gt");
	}

	display.setTextSize(2);
	if (!editing || activeSeq == SEQCV) {
		display.setCursor(1, 11);
		display.print(cvSeqNo + 1);
	}
	if (!editing || activeSeq == SEQGATE) {
		display.setCursor(1, 50);
		display.print(gateSeqNo + 1);
	}
	display.setTextSize(1);

	// Draw a dot if we have a clock high signal
	if (clock.hasSignal()) {
		display.drawFastVLine(127, 0, 1, WHITE);
	}

	//	Draw arrow beneath/above sequence number if selected for editing
	if (editStep == -1) {
		display.drawLine(4, activeSeq == SEQCV ? 34 : 32, 6, activeSeq == SEQCV ? 32 : 34, WHITE);
		display.drawLine(6, activeSeq == SEQCV ? 32 : 34, 8, activeSeq == SEQCV ? 34 : 32, WHITE);
	}

	// Draw the sequence steps for CV and gate sequence
	for (int i = 0; i < 8; i++) {
		int voltHPos = 17 + (i * 14);
		//int voltVPos = 27 - round(cv.seq[cvSeqNo].Steps[i].volts * 5);
		int voltVPos = cvVertPos(cv.seq[cvSeqNo].Steps[i].volts);

		// Draw CV pattern
		if (!editing || activeSeq == SEQCV) {

			//	Show a dot where there is an unplayed step
			if (i + 1 > cv.seq[cvSeqNo].steps) {
				display.fillRect(voltHPos + 5, 30, 1, 1, WHITE);
			}
			else {
				// Draw voltage line 
				if (cv.seq[cvSeqNo].Steps[i].stutter > 0) {
					float w = (float)12 / cv.seq[cvSeqNo].Steps[i].stutter;

					for (int sd = 0; sd < cv.seq[cvSeqNo].Steps[i].stutter; sd++) {
						// draw jagged stripes showing stutter pattern
						display.fillRect(voltHPos + round(sd * w), voltVPos + (sd % 2 ? 0 : 1), round(w), 2, WHITE);
					}
				}
				else {
					display.fillRect(voltHPos + 2, voltVPos, 8, 2, WHITE);
				}

				//	show randomisation by using a vertical dotted line with height proportional to amount of randomisation
				if (cv.seq[cvSeqNo].Steps[i].rand_amt > 0) {
					float randLower = constrain(getRandLimit(cv.seq[cvSeqNo].Steps[i], LOWER), 0, 5);
					float randUpper = constrain(getRandLimit(cv.seq[cvSeqNo].Steps[i], UPPER), 0, 5);
					drawDottedVLine(voltHPos, 2 + cvVertPos(randUpper), 1 + cvVertPos(randLower) - cvVertPos(randUpper), WHITE);
				}
				// draw amount of voltage selected after randomisation applied
				if (cvStep == i) {
					display.fillRect(voltHPos, round(26 - (cvRandVal * 5)), 13, 4, WHITE);
				}
			}
		
		}

		// Draw gate pattern 
		if (!editing || activeSeq == SEQGATE) {

			//	Show a dot where there is an unplayed step
			if (i + 1 > gate.seq[gateSeqNo].steps) {
				display.fillRect(voltHPos + 5, 63, 1, 1, WHITE);
			}
			else {

				if (gate.seq[gateSeqNo].Steps[i].on || gate.seq[gateSeqNo].Steps[i].stutter > 0) {
					if (gateStep == i && !gateRandVal) {
						display.drawRect(voltHPos + 4, 50, 6, 14, WHITE);		// draw gate as empty rectange for current step if set 'on' but randomised 'off'
					}
					else {
						if (gate.seq[gateSeqNo].Steps[i].stutter > 0) {

							// draw base line
							display.drawFastHLine(voltHPos + 3, 63, 8, WHITE);
							float w = (float)8 / gate.seq[gateSeqNo].Steps[i].stutter;
							for (int sd = 0; sd < round((float)gate.seq[gateSeqNo].Steps[i].stutter / 2); sd++) {
								// draw vertical stripes showing stutter layout - if gate is off then stutter starts later														
								display.fillRect(voltHPos + 3 + (gate.seq[gateSeqNo].Steps[i].on ? 0 : round(w)) + (sd * round(w * 2)), 50, round(w), 14, WHITE);
							}
						}
						else {
							display.fillRect(voltHPos + 4, 50, 6, 14, WHITE);
						}
					}
				}
				else {
					display.fillRect(voltHPos + 4, 63, 6, 1, WHITE);
				}
			}

			// draw current step - larger block if 'on' larger base if 'off'
			if (gateStep == i) {
				if (gateRandVal) {
					display.fillRect(voltHPos + 3, 45, 8, 29, WHITE);
				}
				else {
					display.fillRect(voltHPos + 3, 62, 8, 2, WHITE);
				}
			}

			// draw line showing random amount
			uint8_t rndTop = round(gate.seq[gateSeqNo].Steps[i].rand_amt * (float)(24 / 10));
			drawDottedVLine(voltHPos, 64 - rndTop, rndTop, WHITE);
		}

		//	Draw arrow beneath step selected for editing
		if (editStep == i) {
			display.drawLine(voltHPos + 4, activeSeq == SEQCV ? 34 : 32, voltHPos + 6, activeSeq == SEQCV ? 32 : 34, WHITE);
			display.drawLine(voltHPos + 6, activeSeq == SEQCV ? 32 : 34, voltHPos + 8, activeSeq == SEQCV ? 34 : 32, WHITE);
		}
	}

	//	if currently or recently editing show values in bottom area of screen
	if (editing) {
		if (activeSeq == SEQGATE) {
			if (editMode == STEPR || editMode == STEPV || editMode == STUTTER) {
				drawParam("Gate", String(gate.seq[gateSeqNo].Steps[editStep].on ? "ON" : "OFF"), 0, 0, 36, editMode == STEPV);
				drawParam("Random", String(gate.seq[gateSeqNo].Steps[editStep].rand_amt), 38, 0, 44, editMode == STEPR);
				drawParam("Stutter", String(gate.seq[gateSeqNo].Steps[editStep].stutter), 81, 0, 47, editMode == STUTTER);
			}

			if (editMode == LOOPFIRST || editMode == LOOPLAST || editMode == STEPS || editMode == SEQOPT || editMode == RANDALL || editMode == RANDVALS) {
				drawParam("Steps", String(gate.seq[gateSeqNo].steps), 0, 0, 36, editMode == STEPS);
				drawParam("Loop", String(gateLoopFirst + 1) + String(" - ") + String(gateLoopLast + 1), 38, 0, 38, editMode == LOOPFIRST || editMode == LOOPLAST, editMode == LOOPFIRST ? 40 : 64, 9);
				drawParam("Rand", String(editMode == RANDALL ? "All >" : (editMode == RANDVALS ? "Vals > " : "None >")), 80, 0, 42, editMode == SEQOPT || editMode == RANDALL || editMode == RANDVALS, 82, 38);
			}
		}

		if (activeSeq == SEQCV) {
			if (editMode == STEPR || editMode == STEPV || editMode == STUTTER) {
				String v = pitchMode ? pitchFromVolt(cv.seq[cvSeqNo].Steps[editStep].volts) : String(cv.seq[cvSeqNo].Steps[editStep].volts);
				drawParam(pitchMode ? "Pitch" : "Volts", v, 0, 40, 36, editMode == STEPV);
				drawParam("Random", String(cv.seq[cvSeqNo].Steps[editStep].rand_amt), 38, 40, 44, editMode == STEPR);
				drawParam("Stutter", String(cv.seq[cvSeqNo].Steps[editStep].stutter), 81, 40, 47, editMode == STUTTER);
			}

			if (editMode == LOOPFIRST || editMode == LOOPLAST || editMode == STEPS || editMode == SEQOPT || editMode == RANDALL || editMode == RANDVALS) {
				drawParam("Steps", String(cv.seq[cvSeqNo].steps), 0, 40, 36, editMode == STEPS);
				drawParam("Loop", String(cvLoopFirst + 1) + String(" - ") + String(cvLoopLast + 1), 38, 40, 38, editMode == LOOPFIRST || editMode == LOOPLAST, editMode == LOOPFIRST ? 40 : 64, 9);
				drawParam("Rand", String(editMode == RANDALL ? "All >" : (editMode == RANDVALS ? "Vals > " : "None >")), 80, 40, 42, editMode == SEQOPT || editMode == RANDALL || editMode == RANDVALS, 82, 38);
				//display.setCursor(120, 50);
				//display.write(25);		// writes an arrow from the Adafruit library
			}

		}
	}
}


//	returns the vertical position of a voltage line on the cv channel display
int DisplayHandler::cvVertPos(float voltage) {
	return 27 - round(voltage * 5);
}

void DisplayHandler::drawDottedVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
	for (int d = y; d < y + h; d += 3) {
		display.drawPixel(x, d, color);
	}
}

void DisplayHandler::drawParam(String s, String v, uint8_t x, uint8_t y, uint8_t w, boolean selected) {
	drawParam(s, v, x, y, w, selected, 0, 0);
}

void DisplayHandler::drawParam(String s, String v, uint8_t x, uint8_t y, uint8_t w, boolean selected, uint8_t highlightX, uint8_t highlightW) {
	display.setCursor(x + 4, y + 3);
	display.println(s);
	display.setCursor(x + 4, y + 13);
	display.println(v);
	if (selected) {
		display.drawRect(x, y, w, 24, INVERSE);
		if (highlightX > 0) {
			Serial.println(highlightX);
			display.fillRect(highlightX, y + 12, highlightW, 10, INVERSE);
		}
	}
}

//	returns the nearest note name from a given 1v/oct voltage
String DisplayHandler::pitchFromVolt(float v) {
	//uint8_t octave = round(v);
	//uint8_t pitch = round(v * 12) % 12;
	//Serial.print("v: "); Serial.print(v); Serial.print(" v x60: "); Serial.print(round(v * 60)); Serial.print(" p: "); Serial.println(pitch);
	return pitches[round(v * 12) % 12] + (String)int(v);
}

void DisplayHandler::init() {
	const unsigned char diceBitmap[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xCF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x3C, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x7E, 0x03, 0xF0, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x7E, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x1C, 0x7C, 0x07, 0xC0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xC0, 0x00, 0xFE, 0x00, 0xF0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x07, 0x1F, 0x80, 0x7C, 0x7C, 0x18, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xEE, 0x0E, 0x1F, 0x80, 0x00, 0xFC, 0x08, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x03, 0x9C, 0x0F, 0x8F, 0x00, 0xFC, 0x28, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0xF0, 0xF9, 0xF1, 0x80, 0x1F, 0x80, 0x10, 0xC8, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x0F, 0x81, 0xFC, 0x70, 0xF0, 0x1F, 0x8C, 0x01, 0x88, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x7C, 0x01, 0xFC, 0x38, 0x1E, 0x0F, 0x1F, 0x03, 0x18, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x03, 0xE0, 0x01, 0xF0, 0x1F, 0x83, 0xC0, 0x1F, 0x86, 0x18, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x07, 0xC0, 0x7C, 0x1F, 0x0C, 0xD0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x38, 0x7E, 0x00, 0x03, 0xE3, 0xE0, 0x0F, 0x00, 0x19, 0xD0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x60, 0xFE, 0x00, 0x07, 0xF1, 0xE0, 0x03, 0xF0, 0x39, 0xD0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x68, 0xFE, 0x00, 0x07, 0xE0, 0x70, 0x00, 0x7C, 0x31, 0xB0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x2C, 0x30, 0x00, 0x03, 0xC3, 0x30, 0x00, 0x0E, 0x03, 0xA0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x26, 0x00, 0xF0, 0x00, 0x1E, 0x10, 0x00, 0x00, 0x09, 0x20, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x23, 0x03, 0xF8, 0x00, 0xF0, 0x10, 0x00, 0x00, 0xD8, 0x20, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x31, 0x83, 0xF8, 0x07, 0x80, 0x10, 0x00, 0x00, 0xD8, 0x60, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x30, 0xC3, 0xF0, 0x3C, 0x07, 0x90, 0x00, 0x00, 0xB8, 0x60, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x60, 0x01, 0xE0, 0x0F, 0x90, 0x00, 0x00, 0xBB, 0x60, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x30, 0x0F, 0x00, 0x0F, 0x90, 0x00, 0x01, 0xB3, 0x40, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x18, 0x7C, 0x00, 0x1F, 0x90, 0x00, 0x01, 0x87, 0x40, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x08, 0xE0, 0x00, 0x1F, 0x10, 0x00, 0x01, 0x87, 0xC0, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x00, 0x0C, 0x00, 0x0E, 0x10, 0x00, 0x01, 0x06, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x02, 0x3C, 0x00, 0x00, 0x10, 0x00, 0x01, 0x01, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x42, 0x3E, 0x00, 0x00, 0x10, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x0C, 0xE3, 0x7E, 0x00, 0x00, 0x10, 0x00, 0x03, 0x43, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x0C, 0xE3, 0x7E, 0x00, 0x00, 0x10, 0x00, 0x02, 0x66, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x04, 0xF3, 0x7C, 0x00, 0x00, 0x10, 0x00, 0x02, 0xE4, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x04, 0x73, 0x78, 0x07, 0x00, 0x10, 0x00, 0x02, 0xEC, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x06, 0x71, 0x00, 0x0F, 0x80, 0x10, 0x00, 0x02, 0xD8, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x06, 0x71, 0x00, 0x0F, 0x80, 0x18, 0x00, 0x06, 0xD0, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x02, 0x01, 0x80, 0x1F, 0x80, 0x18, 0x03, 0x84, 0xB0, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x03, 0x01, 0x80, 0x1F, 0x01, 0x98, 0x07, 0xC4, 0x20, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01, 0x81, 0x80, 0x1F, 0x03, 0xD8, 0x07, 0xE4, 0x60, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x0E, 0x07, 0xDC, 0x03, 0xE4, 0xC0, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x00, 0x07, 0xDF, 0x03, 0xE4, 0x80, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x60, 0x80, 0x00, 0x0F, 0xDB, 0xE1, 0xE9, 0x80, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x20, 0x80, 0x00, 0x0F, 0x98, 0x7C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x30, 0xC0, 0x00, 0x07, 0x18, 0x0F, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x18, 0x43, 0x80, 0x00, 0x70, 0x03, 0xE6, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x08, 0x47, 0xC0, 0x01, 0xE0, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x0C, 0x4F, 0xC0, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x06, 0x4F, 0xC0, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x06, 0x4F, 0x81, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x03, 0x2F, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
	display.dim(true);
	display.clearDisplay();
	display.setTextColor(WHITE);
	display.drawBitmap(0, 0, diceBitmap, 128, 64, WHITE);
	display.display();
}
