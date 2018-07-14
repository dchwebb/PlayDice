#pragma once
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"
#include "Settings.h"
#include "DisplayHandler.h"
#include "WString.h"
#include <array>
#include <EEPROM.h>

extern CvPatterns cv;
extern GatePatterns gate;
extern editType editMode;
extern uint8_t cvLoopFirst;		// first sequence in loop
extern uint8_t cvLoopLast;		// last sequence in loop
extern uint8_t gateLoopFirst;	// first sequence in loop
extern uint8_t gateLoopLast;	// last sequence in loop
extern uint32_t lastEditing;
extern boolean autoSave;		// set to true if autosave enabled
extern boolean saveRequired;	// set to true after editing a parameter needing a save (saves batched to avoid too many writes)
extern void checkEditState();
extern void normalMode();
extern void initCvSequence(int seqNum, seqInitType initType, uint16_t numSteps);
extern void initGateSequence(int seqNum, seqInitType initType, uint16_t numSteps);

std::array<MenuItem, 7> menu{ { { 0, "< Back", 1 },{ 1, "Save" },{ 2, "Load" },{ 3, "LFO Mode" },{ 4, "Noise Mode" },{ 5, "Init All" },{ 6, "Autosave", 0, "N" } } };


class SetupMenu {
public:
	void menuPicker(int action);
	uint8_t size();
	String menuName(uint8_t n);
	boolean menuSelected(uint8_t n);
	String menuVal(uint8_t n);
	void setVal(String name, String val);
	void saveSettings();
	boolean loadSettings();		// returns true if settings found in EEPROM
private:
	long clockSignal;
	int32_t frameStart;
	void romWrite(uint16_t pos, uint8_t val);
	uint8_t romRead(uint16_t pos);

};

String SetupMenu::menuName(uint8_t n) {
	return menu[n].name;
}

boolean SetupMenu::menuSelected(uint8_t n) {
	return menu[n].selected;
}

String SetupMenu::menuVal(uint8_t n) {
	return menu[n].val;
}

uint8_t SetupMenu::size() {
	return menu.size();
}

void SetupMenu::setVal(String name, String val) {
	// Sets the value of a menu item by name
	for (uint8_t m = 0; m < menu.size(); m++) {
		if (menu[m].name == name) {
			menu[m].val = val;
		}
	}
}

// carry out the screen refresh building the various UI elements
void SetupMenu::menuPicker(int action) {

	if (action == ENCUP) {
		for (int m = menu.size() - 2; m > -1; m--) {
			if (menu[m].selected) {
				menu[m].selected = 0;
				menu[m + 1].selected = 1;
			}
		}
	}

	if (action == ENCDN) {
		for (unsigned int m = 1; m < menu.size(); m++) {
			if (menu[m].selected) {
				menu[m].selected = 0;
				menu[m - 1].selected = 1;
			}
		}
	}

	if (action == ENCODER) {
		for (unsigned int m = 0; m < menu.size(); m++) {
			if (menu[m].selected) {
				Serial.println(menu[m].name);
				if (menu[m].name == "< Back") {
					normalMode();
				}
				else if (menu[m].name == "Save") {
					saveSettings();
					normalMode();
				}
				else if (menu[m].name == "Load") {
					loadSettings();
					normalMode();
				}
				else if (menu[m].name == "LFO Mode") {
					editMode = LFO;
					if (autoSave) {
						saveSettings();
					}
				}
				else if (menu[m].name == "Noise Mode") {
					editMode = NOISE;
					if (autoSave) {
						saveSettings();
					}
				}
				else if (menu[m].name == "Init All") {
					for (int p = 0; p < 8; p++) {
						initCvSequence(p, INITRAND, 8);
						srand(micros());
						initGateSequence(p, INITRAND, 8);
					}
					normalMode();
				}
				else if (menu[m].name == "Autosave") {
					autoSave = !autoSave;
					saveSettings();
					menu[m].val = autoSave ? "Y" : "N";
				}
			}
		}
	}
}

void SetupMenu::romWrite(uint16_t pos, uint8_t val) {
	if (EEPROM.read(pos) != val) {
		EEPROM.write(pos, val);
		Serial.print("write: "); Serial.print(pos); Serial.print(": "); Serial.println(val);
	}
}

uint8_t SetupMenu::romRead(uint16_t pos) {
	return EEPROM.read(pos);
}

void SetupMenu::saveSettings() {
	saveRequired = 0;

	// write variables and cv/gate structs to EEPROM (2048 bytes in Teensy 3.2)
	// to provide some future proofing store variables from 0, cv struct (est 544 bytes) from 500, gate struct (est 144 bytes from 1500)

	//	Basic header to check if settings are saved - ASCII values of 'PD' followed by version
	romWrite(0, 80);
	romWrite(1, 68);
	romWrite(2, 1);

	romWrite(3, cvLoopFirst);		// first sequence in loop
	romWrite(4, cvLoopLast);		// last sequence in loop
	romWrite(5, gateLoopFirst);		// first sequence in loop
	romWrite(6, gateLoopLast);		// last sequence in loop

	romWrite(7, (editMode == LFO));		// LFO Mode
	romWrite(8, (editMode == NOISE));	// Noise Mode

	romWrite(9, autoSave);	// Noise Mode

	// Serialise cv struct
	char cvToByte[sizeof(cv)];
	memcpy(cvToByte, &cv, sizeof(cv));
	for (uint16_t b = 0; b < sizeof(cv); b++) {		// Write cv array from position 500
		romWrite(b + 500, cvToByte[b]);
	}

	// Serialise gate struct
	char gateToByte[sizeof(gate)];
	memcpy(gateToByte, &gate, sizeof(gate));
	for (uint16_t b = 0; b < sizeof(gate); b++) {		// Write gate array from position 1500
		romWrite(b + 1500, gateToByte[b]);
	}

}

boolean SetupMenu::loadSettings() {

	if (romRead(0) != 80 || romRead(1) != 68 || romRead(2) != 1) {
		Serial.println("Read Error - header corrupt");
		return 0;
	}

	cvLoopFirst = romRead(3);		// first sequence in loop
	cvLoopLast = romRead(4);		// last sequence in loop
	gateLoopFirst = romRead(5);		// first sequence in loop
	gateLoopLast = romRead(6);		// last sequence in loop

	if (romRead(7)) {
		editMode = LFO;
	}
	else if (romRead(8)) {
		editMode = NOISE;
	}
	autoSave = romRead(9);
	setVal("Autosave", autoSave ? "Y" : "N");

	// deserialise cv struct
	char cvToByte[sizeof(cv)];
	for (uint16_t b = 0; b < sizeof(cv); b++) {
		cvToByte[b] = romRead(b + 500);
	}
	memcpy(&cv, cvToByte, sizeof(cv));

	// deserialise gate struct
	char gateToByte[sizeof(gate)];
	for (uint16_t b = 0; b < sizeof(gate); b++) {
		gateToByte[b] = romRead(b + 1500);
	}
	memcpy(&gate, gateToByte, sizeof(gate));

	return 1;
}