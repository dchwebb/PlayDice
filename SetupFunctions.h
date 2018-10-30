#pragma once
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"
//#include "Settings.h"
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
extern void makeQuantiseArray();
extern String const pitches[];
extern uint8_t submenuSize;		// number of items in array used to pick from submenu items
extern uint8_t submenuVal;		// currently selected submenu item
extern actionOpts actionCVType;
extern actionOpts actionBtnType;
extern int8_t cvOffset;
const String *submenuArray;		// Stores a pointer to the array used to select submenu choices

std::array<MenuItem, 13> menu{ { { 0, "LFO Mode", 1 },{ 1, "Noise Mode" },{ 2, "Action CV", 0, actions[0] },{ 3, "Action Btn", 0, actions[0] },
{ 4, "Autosave", 0, OffOnOpts[0] },{ 5, "Init All" },{ 6, "Save Settings" },{ 7, "Load Settings" },{ 8, "CV Calibration", 0, "0" } } };

class SetupMenu {
public:
	void menuPicker(int action);
	uint8_t size();
	String menuName(uint8_t n);
	boolean menuSelected(uint8_t n);
	String menuCurrent();		// name of currently selected menu item
	String menuVal(uint8_t n);
	void setVal(String name, String val);
	void saveSettings();
	boolean loadSettings();		// returns true if settings found in EEPROM
	boolean numberEdit;			// true if submenu function is editing number
private:
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

String SetupMenu::menuCurrent() {
	// Sets the value of a menu item by name
	for (uint8_t m = 0; m < menu.size(); m++) {
		if (menu[m].selected) {
			return menu[m].name;
		}
	}
	return "";
}

// carry out the screen refresh building the various UI elements
void SetupMenu::menuPicker(int action) {

	if (editMode == SUBMENU) {
		if (menuCurrent() == "CV Calibration") {
			if (action == ENCUP) {
				cvOffset += 1;
			}
			else if (action == ENCDN) {
				cvOffset -= 1;
			}
			setVal("CV Calibration", cvOffset);
		}
		else if (action == ENCUP && submenuVal < submenuSize - 1) {
			submenuVal += 1;
		} 
		else if (action == ENCDN && submenuVal > 0) {
			submenuVal -= 1;
		}
		if (action == ENCODER) {
			editMode = SETUP;
			numberEdit = 0;
			
			// store value back after submenu editing
			for (uint8_t m = 0; m < menu.size(); m++) {
				if (menu[m].selected) {
					if (menu[m].name == "Action CV") {
						actionCVType = (actionOpts)submenuVal;
						setVal(menu[m].name, actions[actionCVType]);
					}
					if (menu[m].name == "Action Btn") {
						actionBtnType = (actionOpts)submenuVal;
						setVal(menu[m].name, actions[actionBtnType]);
					}
				}
			}
			saveSettings();
		}
	}
	else {
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
			for (uint8_t m = 0; m < menu.size(); m++) {
				if (menu[m].selected) {
					Serial.println(menu[m].name);
					if (menu[m].name == "< Back") {
						normalMode();
					}
					else if (menu[m].name == "Save Settings") {
						saveSettings();
						normalMode();
					}
					else if (menu[m].name == "Load Settings") {
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
						menu[m].val = OffOnOpts[autoSave];
					}
					else if (menu[m].name == "Action CV") {
						submenuArray = actions;
						submenuSize = 3;				// can't find way of checking this dynamically
						submenuVal = actionCVType;
						editMode = SUBMENU;
					}
					else if (menu[m].name == "Action Btn") {
						submenuArray = actions;
						submenuSize = 3;				// can't find way of checking this dynamically
						submenuVal = actionBtnType;
						editMode = SUBMENU;
					}
					else if (menu[m].name == "CV Calibration") {
						editMode = SUBMENU;
						numberEdit = 1;
						setVal("CV Calibration", cvOffset);
					}
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

	romWrite(9, autoSave);
	//romWrite(10, pitchMode);
	//romWrite(11, quantRoot);
	//romWrite(12, quantScale);

	romWrite(13, actionCVType);
	romWrite(14, actionBtnType);

	//romWrite(15, triggerMode);

	romWrite(16, cvOffset);

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
	setVal("Autosave", OffOnOpts[autoSave]);
	setVal("Action CV", actions[actionCVType]);
	actionBtnType = (actionOpts)romRead(14);
	setVal("Action Btn", actions[actionBtnType]);
	cvOffset = romRead(16);
	setVal("CV Calibration", cvOffset);
	

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