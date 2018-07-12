#pragma once
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"
#include "Settings.h"
#include "WString.h"
#include <array>
#include <EEPROM.h>

extern editType editMode;
extern uint8_t cvLoopFirst;		// first sequence in loop
extern uint8_t cvLoopLast;		// last sequence in loop
extern uint8_t gateLoopFirst;	// first sequence in loop
extern uint8_t gateLoopLast;	// last sequence in loop
extern uint32_t lastEditing;
extern void checkEditState();

std::array<MenuItem, 4> menu{ { { 0, "< Back", 1 },{ 1, "Save" },{ 2, "Load" },{ 3, "LFO Mode" } } };


class SetupMenu {
public:
	void menuPicker(int action);
	void saveSettings();
	void loadSettings();
private:
	long clockSignal;
	int32_t frameStart;
};




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
					editMode = STEPV;
					checkEditState();
					lastEditing = 0;
				}
				else if (menu[m].name == "Save") {
					saveSettings();
					editMode = STEPV;
					checkEditState();
					lastEditing = 0;
				}
				else if (menu[m].name == "Load") {
					loadSettings();
					editMode = STEPV;
					checkEditState();
					lastEditing = 0;
				}
				else if (menu[m].name == "LFO Mode") {
					editMode = LFO;
				}
			}
		}
	}
}


void SetupMenu::saveSettings() {
	static uint8_t settings[2048];

	// write variables and cv/gate structs to EEPROM (2048 bytes in Teensy 3.2)
	// to provide some future proofing store variables from 0, cv struct (est 544 bytes) from 500, gate struct (est 144 bytes from 1500)
	
	settings[0] = cvLoopFirst;		// first sequence in loop
	settings[1] = cvLoopLast;		// last sequence in loop
	settings[2] = gateLoopFirst;	// first sequence in loop
	settings[3] = gateLoopLast;		// last sequence in loop

	for (int b = 0; b < 4; b++) {
		EEPROM.write(b, settings[b]);
	}


	// Serialise cv struct
	char cvToByte[sizeof(cv)];
	memcpy(cvToByte, &cv, sizeof(cv));
	//	Serial.print("Cv Struct: "); Serial.print(sizeof(cv)); Serial.print(" cvToByte Struct: "); Serial.println(sizeof(cvToByte));
	for (int b = 0; b < sizeof(cv); b++) {		// Write cv array from position 500
		EEPROM.write(b + 500, cvToByte[b]);
	}


	// Serialise gate struct
	char gateToByte[sizeof(gate)];
	memcpy(gateToByte, &gate, sizeof(gate));
	//	Serial.print("Cv Struct: "); Serial.print(sizeof(gate)); Serial.print(" gateToByte Struct: "); Serial.println(sizeof(gateToByte));
	for (int b = 0; b < sizeof(gate); b++) {		// Write gate array from position 1500
		EEPROM.write(b + 1500, gateToByte[b]);
	}

}

void SetupMenu::loadSettings() {
	static uint8_t settings[2048];

	for (int b = 0; b < 4; b++) {
		settings[b] = EEPROM.read(b);
	}
	cvLoopFirst = settings[0];		// first sequence in loop
	cvLoopLast = settings[1];		// last sequence in loop
	gateLoopFirst = settings[2];	// first sequence in loop
	gateLoopLast = settings[3];		// last sequence in loop

	// deserialise cv struct
	char cvToByte[sizeof(cv)];
	for (int b = 0; b < sizeof(cv); b++) {
		cvToByte[b] = EEPROM.read(b + 500);
	}
	memcpy(&cv, cvToByte, sizeof(cv));

	// deserialise gate struct
	char gateToByte[sizeof(gate)];
	for (int b = 0; b < sizeof(gate); b++) {
		gateToByte[b] = EEPROM.read(b + 1500);
	}
	memcpy(&gate, gateToByte, sizeof(gate));

}