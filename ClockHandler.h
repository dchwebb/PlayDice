// Code to manage external clock reading - Eurorack clock fires 16 5V pulses per bar
#include "Settings.h"

extern uint16_t bpm;

class ClockHandler {
public:
	ClockHandler(int l_minBPM, int l_maxBPM) {
		minBPM = l_minBPM;
		maxBPM = l_maxBPM;
	};

	int clockThreshold = 500;		// Clock is converted to value between 0 and 1023 for 0-3.3V - set threshold to converted level
	int clockBPM = 0;				// BPM read from external clock
	uint32_t clockHighTime = 0;		// time in milliseconds of last clock signal (eg for timing pulses and display)
	uint32_t clockInterval = 0;		// time in milliseconds of current clock interval
	boolean hasSignal();			// returns true if a clock signal is detected and within sensible limits
	int readClock();				// reads the clock pin and calculates BPM if clock signal found
	void printDebug();				// prints debug information to the serial monitor

private:
	int minBPM = 35;				// minimum BPM allowed for internal/external clock
	int maxBPM = 300;				// maximum BPM allowed for internal/external clock
	boolean clockSignal = 0;		// 1 = External clock is sending currently sending pulses
	int clockInput = 0;				// voltage reading of clock inpu pin translated to 0-1023 range (0-3.3V)
	boolean clockHigh = 0;			// Set to 1 if clock is above threshold
	uint32_t lastClockHigh = 0;		// time in milliseconds since clock last high to check for bounce
	uint32_t lastGoodBPM = 0;		// time in milliseconds since we got a valid BPM reading to allow brief dropouts to be handled
	int testClockBPM = 0;			// Provisional BPM read from external clock - may not be used for actual clock if signal intermittant
	static const int avStepsBMP = 5;// TODO - number of previous reads to average
	int previousBPM[avStepsBMP];	// TODO - use array to average minor tempo fluctuations out
	int counterPrevBPM = 0;			// TODO - iterates through BPM averager
};

int ClockHandler::readClock() {

	if (!digitalRead(CLOCKPIN)) {

		// check if a new clock pulse has been detected - previous state low and no high clock in the last 20 milliseconds ( && millis() - lastClockHigh > 20 last test causing some false readings so maybe ditch)
		if (clockHigh == 0 && millis() - clockHighTime > 20) {
			//	Eurorack clock fires 16 5V pulses per bar
			clockInterval = millis() - clockHighTime;
			testClockBPM = round((float)(1 / (((double)(millis() - clockHighTime) / 1000) * 4)) * 60);

			clockHighTime = millis();
			clockSignal = 1;

#if DEBUGCLOCK
			Serial.print("High  "); Serial.print(" ms: "); Serial.println(millis());
#endif
		}
		clockHigh = 1;
		lastClockHigh = millis();
	}
	else {
		clockHigh = 0;

	}

	//	check if clock signal is in BPM limits
	if (testClockBPM > 0 && clockSignal && testClockBPM >= minBPM && testClockBPM < maxBPM) {

		// BPM averager to smooth out tempo variations
		previousBPM[(int)counterPrevBPM % avStepsBMP] = testClockBPM;			//	add BPM to averager array using a modulus to shift position each clock

		counterPrevBPM++;
		int AvBPM = 0;
		for (int i = 0; i < avStepsBMP; i++) {
			AvBPM += previousBPM[i];
		}
		AvBPM = round((float)AvBPM / avStepsBMP);

#if DEBUGCLOCK
		Serial.print("BPM: "); Serial.print(testClockBPM); Serial.print(" Av: "); Serial.print(AvBPM); Serial.print(" [");
		Serial.print(previousBPM[0]); Serial.print(" "); Serial.print(previousBPM[1]); Serial.print(" "); Serial.print(previousBPM[2]); Serial.print(" "); Serial.print(previousBPM[3]); Serial.print(" "); Serial.print(previousBPM[4]); Serial.println("]");
#endif
		//	check if averager looks good enough to use
		if (counterPrevBPM > avStepsBMP && AvBPM != testClockBPM && abs(AvBPM - testClockBPM) < 3) {
			testClockBPM = AvBPM;
		}


		clockBPM = testClockBPM;
		lastGoodBPM = millis();
		testClockBPM = 0;
	}

#if DEBUGCLOCK
	if (testClockBPM > 0 && millis() - clockHighTime < 10) {
		Serial.print("Dropped  "); Serial.print(" ms: "); Serial.println(millis());
	}
#endif

	//	if clock signal has not fired or no good BPM reading in the last second clear BPM
	if (millis() - clockHighTime > 1000 || millis() - lastGoodBPM > 1000) {
		clockSignal = 0;
		clockBPM = 0;
	}

	return clockBPM;
};

//	Debug missing clock signals
void ClockHandler::printDebug() {

	if (clockBPM >= minBPM && clockBPM < maxBPM && clockSignal) {
		Serial.print("Clock");
	}
	else {
		Serial.print("No Clock");
	}

	Serial.print(" ms: "); Serial.print(millis());
	Serial.print(" clk bpm: "); Serial.print(clockBPM);
	Serial.print(" act bpm: "); Serial.println(bpm);
}



boolean ClockHandler::hasSignal() {
	return clockSignal;
}

