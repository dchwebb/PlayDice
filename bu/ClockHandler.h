// Code to manage external clock reading

class ClockHandler {
public:
	ClockHandler(int l_minBPM, int l_maxBPM) {
		minBPM = l_minBPM;
		maxBPM = l_maxBPM;
	};

	int clockInput = 0;				// Reading from clock input
	int clockThreshold = 500;		// Clock is converted to value between 0 and 1023 for 0-3.3V - set threshold to converted level
	boolean clockHigh = 0;				// Set to 1 if clock is above threshold
	unsigned long lastClockHigh = 0;// time in milliseconds since clock last high to check for bounce
	unsigned long clockHighTime = 0;// time in milliseconds of last clock signal
	unsigned long clockInterval = 0;// time in milliseconds of current clock interval
	int clockBPM = 0;				// BPM read from external clock
	boolean hasSignal();
	int readClock();

private:
	int minBPM = 35;				// minimum BPM allowed for internal/external clock
	int maxBPM = 300;				// maximum BPM allowed for internal/external clock
	boolean clockSignal = 0;			// 1 = External clock is sending currently sending pulses
};

int ClockHandler::readClock() {

	if (clockInput > clockThreshold) {
		// check if a new clock pulse has been detected - previous state low and no high clock in the last 20 milliseconds ( && millis() - lastClockHigh > 20 last test causing some false readings so maybe ditch)
		if (clockHigh == 0 && millis() - lastClockHigh > 20) {
			//	Eurorack clock fires 16 5V pulses per bar
			clockInterval = millis() - clockHighTime;
			clockBPM = (1 / (((double)(millis() - clockHighTime) / 1000) * 4)) * 60;
			clockHighTime = millis();
			clockSignal = 1;

			Serial.println("High");
		}
		clockHigh = 1;
		lastClockHigh = millis();
	}
	else {
		clockHigh = 0;
	}
	//	check if clock signal has fired in the last second
	if (millis() - clockHighTime > 1000) {
		clockSignal = 0;
	}

	return clockBPM;

}

boolean ClockHandler::hasSignal() {
	return clockSignal;
}

