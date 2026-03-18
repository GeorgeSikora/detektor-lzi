#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_BloodOxygen_S.h>

namespace {
constexpr uint8_t GSR_PIN = A0;
constexpr uint8_t ECG_PIN = A1;
constexpr uint8_t ECG_LO_MINUS_PIN = 2;
constexpr uint8_t ECG_LO_PLUS_PIN = 3;
constexpr uint8_t BUZZER_PIN = 5;
constexpr uint8_t START_BUTTON_PIN = 8;
constexpr uint8_t STOP_BUTTON_PIN = 9;
constexpr uint8_t LCD_TEXT_ADDR = 0x3E;
constexpr uint8_t LCD_BACKLIGHT_ADDR_PRIMARY = 0x62;
constexpr uint8_t LCD_BACKLIGHT_ADDR_ALT = 0x60;
constexpr uint8_t OXIMETER_ADDR = 0x57;

constexpr uint8_t LCD_COLS = 16;
constexpr uint8_t LCD_ROWS = 2;

constexpr unsigned long GSR_SAMPLE_INTERVAL_MS = 250;
constexpr unsigned long ECG_SAMPLE_INTERVAL_MS = 4;
constexpr unsigned long DISPLAY_REFRESH_INTERVAL_MS = 500;
constexpr unsigned long SERIAL_PRINT_INTERVAL_MS = 1000;
constexpr unsigned long OXIMETER_READ_INTERVAL_MS = 4000;
constexpr unsigned long OXIMETER_RETRY_INTERVAL_MS = 5000;
constexpr unsigned long ECG_MIN_BEAT_INTERVAL_MS = 300;
constexpr unsigned long ECG_MAX_BEAT_INTERVAL_MS = 1500;
constexpr unsigned long ECG_SIGNAL_TIMEOUT_MS = 3000;
constexpr unsigned long BUZZER_BEEP_DURATION_MS = 30;
constexpr unsigned int BUZZER_FREQUENCY_HZ = 2400;
constexpr unsigned long BUZZER_TEST_DURATION_MS = 180;
constexpr unsigned int BUZZER_TEST_FREQUENCY_HZ = 1800;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long STARTUP_NOTE_DURATION_MS = 85;
constexpr unsigned long MODE_SWITCH_NOTE_DURATION_MS = 55;
constexpr unsigned long TONE_GAP_DURATION_MS = 20;
constexpr unsigned long BASELINE_CAPTURE_MS = 8000;
constexpr int HR_LIE_DELTA_BPM = 12;
constexpr int GSR_LIE_DELTA = 45;

constexpr uint8_t MENU_BACKLIGHT_RED = 255;
constexpr uint8_t MENU_BACKLIGHT_GREEN = 180;
constexpr uint8_t MENU_BACKLIGHT_BLUE = 0;
constexpr uint8_t PAUSED_BACKLIGHT_RED = 255;
constexpr uint8_t PAUSED_BACKLIGHT_GREEN = 200;
constexpr uint8_t PAUSED_BACKLIGHT_BLUE = 0;

constexpr uint8_t LCD_CLEARDISPLAY = 0x01;
constexpr uint8_t LCD_RETURNHOME = 0x02;
constexpr uint8_t LCD_ENTRYMODESET = 0x04;
constexpr uint8_t LCD_DISPLAYCONTROL = 0x08;
constexpr uint8_t LCD_FUNCTIONSET = 0x20;
constexpr uint8_t LCD_SETDDRAMADDR = 0x80;

constexpr uint8_t LCD_ENTRYLEFT = 0x02;
constexpr uint8_t LCD_ENTRYSHIFTDECREMENT = 0x00;
constexpr uint8_t LCD_DISPLAYON = 0x04;
constexpr uint8_t LCD_CURSOROFF = 0x00;
constexpr uint8_t LCD_BLINKOFF = 0x00;
constexpr uint8_t LCD_8BITMODE = 0x10;
constexpr uint8_t LCD_2LINE = 0x08;
constexpr uint8_t LCD_5X8DOTS = 0x00;

constexpr uint8_t RGB_REG_MODE1 = 0x00;
constexpr uint8_t RGB_REG_MODE2 = 0x01;
constexpr uint8_t RGB_REG_OUTPUT = 0x08;
constexpr uint8_t RGB_REG_RED = 0x04;
constexpr uint8_t RGB_REG_GREEN = 0x03;
constexpr uint8_t RGB_REG_BLUE = 0x02;

class RgbLcd : public Print {
public:
	bool begin(uint8_t columns, uint8_t rows, uint8_t textAddress, uint8_t backlightAddress) {
		_columns = columns;
		_rows = rows;
		_textAddress = textAddress;
		_backlightAddress = backlightAddress;
		_available = probe(_textAddress) && probe(_backlightAddress);
		if (!_available) {
			return false;
		}

		delay(50);
		writeCommand(LCD_FUNCTIONSET | LCD_8BITMODE | LCD_2LINE | LCD_5X8DOTS);
		delayMicroseconds(4500);
		writeCommand(LCD_FUNCTIONSET | LCD_8BITMODE | LCD_2LINE | LCD_5X8DOTS);
		delayMicroseconds(150);
		writeCommand(LCD_FUNCTIONSET | LCD_8BITMODE | LCD_2LINE | LCD_5X8DOTS);
		writeCommand(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
		clear();
		writeCommand(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
		home();

		writeBacklightRegister(RGB_REG_MODE1, 0x00);
		writeBacklightRegister(RGB_REG_OUTPUT, 0xFF);
		writeBacklightRegister(RGB_REG_MODE2, 0x20);
		setRGB(0, 80, 160);
		return true;
	}

	void clear() {
		if (!_available) {
			return;
		}
		writeCommand(LCD_CLEARDISPLAY);
		delayMicroseconds(2000);
	}

	void home() {
		if (!_available) {
			return;
		}
		writeCommand(LCD_RETURNHOME);
		delayMicroseconds(2000);
	}

	void setCursor(uint8_t col, uint8_t row) {
		if (!_available) {
			return;
		}
		static const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
		if (row >= _rows) {
			row = _rows - 1;
		}
		writeCommand(LCD_SETDDRAMADDR | (col + rowOffsets[row]));
	}

	void setRGB(uint8_t red, uint8_t green, uint8_t blue) {
		if (!_available) {
			return;
		}
		writeBacklightRegister(RGB_REG_RED, red);
		writeBacklightRegister(RGB_REG_GREEN, green);
		writeBacklightRegister(RGB_REG_BLUE, blue);
	}

	size_t write(uint8_t value) override {
		if (!_available) {
			return 0;
		}
		Wire.beginTransmission(_textAddress);
		Wire.write(0x40);
		Wire.write(value);
		return Wire.endTransmission() == 0 ? 1 : 0;
	}

	bool available() const {
		return _available;
	}

private:
	bool probe(uint8_t address) {
		Wire.beginTransmission(address);
		return Wire.endTransmission() == 0;
	}

	void writeCommand(uint8_t value) {
		Wire.beginTransmission(_textAddress);
		Wire.write(0x80);
		Wire.write(value);
		Wire.endTransmission();
	}

	void writeBacklightRegister(uint8_t reg, uint8_t value) {
		Wire.beginTransmission(_backlightAddress);
		Wire.write(reg);
		Wire.write(value);
		Wire.endTransmission();
	}

	uint8_t _columns = 0;
	uint8_t _rows = 0;
	uint8_t _textAddress = LCD_TEXT_ADDR;
	uint8_t _backlightAddress = LCD_BACKLIGHT_ADDR_PRIMARY;
	bool _available = false;
};

enum class DeviceMode : uint8_t {
	Standby,
	Measuring,
	Paused,
};

enum class Verdict : uint8_t {
	Calibrating,
	Truth,
	Lie,
};

RgbLcd lcd;
DFRobot_BloodOxygen_S_I2C oximeter(&Wire, OXIMETER_ADDR);

bool lcdReady = false;
bool oximeterReady = false;
uint8_t lcdBacklightAddress = LCD_BACKLIGHT_ADDR_PRIMARY;
DeviceMode deviceMode = DeviceMode::Standby;
Verdict verdict = Verdict::Calibrating;

int gsrValue = 0;
int ecgValue = 0;
int ecgHeartRate = -1;
int oximeterHeartRate = -1;
int heartRate = -1;
int spo2 = -1;
int boardTempC = -1;
bool ecgLeadsOff = true;
bool ecgTrackingReady = false;
bool ecgPeakArmed = false;
bool buzzerActive = false;
bool buzzerEnabled = true;

int ecgFiltered = 0;
int ecgBaseline = 512;
int ecgEnvelope = 0;
int ecgPreviousFiltered = 0;
int ecgPreviousDelta = 0;
int baselineGsr = -1;
int baselineHeartRate = -1;
long baselineGsrSum = 0;
long baselineHeartRateSum = 0;
uint16_t baselineGsrSamples = 0;
uint16_t baselineHeartRateSamples = 0;

unsigned long lastGsrSampleMs = 0;
unsigned long lastEcgSampleMs = 0;
unsigned long lastDisplayRefreshMs = 0;
unsigned long lastSerialPrintMs = 0;
unsigned long lastOximeterReadMs = 0;
unsigned long lastOximeterRetryMs = 0;
unsigned long lastEcgBeatMs = 0;
unsigned long lastValidEcgBeatMs = 0;
unsigned long lastBuzzerStartMs = 0;
unsigned long buzzerDurationMs = 0;
unsigned long lastStartButtonChangeMs = 0;
unsigned long lastStopButtonChangeMs = 0;
unsigned long measurementStartedMs = 0;

bool lastStartButtonReading = HIGH;
bool lastStopButtonReading = HIGH;
bool startButtonState = HIGH;
bool stopButtonState = HIGH;

void refreshDisplay();
void updateBacklight();
void updateVerdict(unsigned long now);

bool probeI2C(uint8_t address) {
	Wire.beginTransmission(address);
	return Wire.endTransmission() == 0;
}

void printAddress(uint8_t address) {
	Serial.print(F("0x"));
	if (address < 0x10) {
		Serial.print('0');
	}
	Serial.print(address, HEX);
}

void scanI2CBus() {
	Serial.println();
	Serial.println(F("=== I2C scan start ==="));
	for (uint8_t address = 1; address < 127; ++address) {
		if (!probeI2C(address)) {
			continue;
		}
		Serial.print(F("Found device at "));
		printAddress(address);
		if (address == LCD_TEXT_ADDR) {
			Serial.print(F(" -> LCD1602 controller"));
		} else if (address == LCD_BACKLIGHT_ADDR_PRIMARY || address == LCD_BACKLIGHT_ADDR_ALT) {
			Serial.print(F(" -> LCD1602 RGB backlight"));
		} else if (address == OXIMETER_ADDR) {
			Serial.print(F(" -> Gravity HR + SpO2"));
		}
		Serial.println();
	}
	Serial.println(F("=== I2C scan end ==="));
}

void writeLcdLine(uint8_t row, const char *text) {
	if (!lcd.available()) {
		return;
	}

	lcd.setCursor(0, row);
	const size_t length = strlen(text);
	for (uint8_t i = 0; i < LCD_COLS; ++i) {
		lcd.write(i < length ? text[i] : ' ');
	}
	}

void showStatusScreen(const char *secondLine) {
	if (!lcdReady) {
		return;
	}
	writeLcdLine(0, "Detektor lzi");
	writeLcdLine(1, secondLine);
	}

int sampleGsr() {
	long total = 0;
	for (uint8_t index = 0; index < 8; ++index) {
		total += analogRead(GSR_PIN);
		delayMicroseconds(300);
	}
	return static_cast<int>(total / 8);
	}

bool isEcgLeadOff() {
	return digitalRead(ECG_LO_MINUS_PIN) == HIGH || digitalRead(ECG_LO_PLUS_PIN) == HIGH;
	}

void syncHeartRateSource() {
	if (ecgHeartRate > 0) {
		heartRate = ecgHeartRate;
	} else {
		heartRate = oximeterHeartRate;
	}
	}

void startBuzzerTone(unsigned int frequencyHz, unsigned long durationMs, unsigned long now) {
	if (!buzzerEnabled) {
		return;
	}
	tone(BUZZER_PIN, frequencyHz);
	buzzerActive = true;
	lastBuzzerStartMs = now;
	buzzerDurationMs = durationMs;
	}

void startBeatBeep(unsigned long now) {
	startBuzzerTone(BUZZER_FREQUENCY_HZ, BUZZER_BEEP_DURATION_MS, now);
	}

void triggerBuzzerTest(unsigned long now) {
	startBuzzerTone(BUZZER_TEST_FREQUENCY_HZ, BUZZER_TEST_DURATION_MS, now);
	}

void playToneStep(unsigned int frequencyHz, unsigned long durationMs) {
	if (!buzzerEnabled) {
		return;
	}
	tone(BUZZER_PIN, frequencyHz);
	delay(durationMs);
	noTone(BUZZER_PIN);
	delay(TONE_GAP_DURATION_MS);
	}

void playStartupMelody() {
	playToneStep(1047, STARTUP_NOTE_DURATION_MS);
	playToneStep(1319, STARTUP_NOTE_DURATION_MS);
	playToneStep(1568, STARTUP_NOTE_DURATION_MS);
	playToneStep(1760, STARTUP_NOTE_DURATION_MS);
	playToneStep(2093, STARTUP_NOTE_DURATION_MS + 10);
	playToneStep(1760, STARTUP_NOTE_DURATION_MS);
	playToneStep(1568, STARTUP_NOTE_DURATION_MS + 20);
	}

void playStartMeasurementTone() {
	playToneStep(1480, MODE_SWITCH_NOTE_DURATION_MS);
	playToneStep(1760, MODE_SWITCH_NOTE_DURATION_MS + 10);
	}

void playPauseMeasurementTone() {
	playToneStep(1760, MODE_SWITCH_NOTE_DURATION_MS);
	playToneStep(1397, MODE_SWITCH_NOTE_DURATION_MS + 10);
	}

void updateBuzzer(unsigned long now) {
	if (!buzzerActive) {
		return;
	}
	if (now - lastBuzzerStartMs >= buzzerDurationMs) {
		noTone(BUZZER_PIN);
		buzzerActive = false;
		buzzerDurationMs = 0;
	}
	}

void stopBuzzer() {
	if (!buzzerActive) {
		return;
	}
	noTone(BUZZER_PIN);
	buzzerActive = false;
	buzzerDurationMs = 0;
	}

void resetEcgTracking() {
	ecgTrackingReady = false;
	ecgPeakArmed = false;
	ecgHeartRate = -1;
	ecgEnvelope = 0;
	ecgPreviousDelta = 0;
	lastEcgBeatMs = 0;
	lastValidEcgBeatMs = 0;
	stopBuzzer();
	syncHeartRateSource();
	}

void resetMeasurementValues() {
	gsrValue = 0;
	ecgValue = 0;
	ecgHeartRate = -1;
	oximeterHeartRate = -1;
	heartRate = -1;
	spo2 = -1;
	boardTempC = -1;
	ecgFiltered = 0;
	ecgBaseline = 512;
	ecgEnvelope = 0;
	ecgPreviousFiltered = 0;
	ecgPreviousDelta = 0;
	ecgLeadsOff = true;
	baselineGsr = -1;
	baselineHeartRate = -1;
	baselineGsrSum = 0;
	baselineHeartRateSum = 0;
	baselineGsrSamples = 0;
	baselineHeartRateSamples = 0;
	measurementStartedMs = 0;
	verdict = Verdict::Calibrating;
	resetEcgTracking();
	}

void setDeviceMode(DeviceMode newMode) {
	if (deviceMode == newMode) {
		return;
	}

	deviceMode = newMode;
	if (deviceMode == DeviceMode::Measuring) {
		resetMeasurementValues();
		measurementStartedMs = millis();
		Serial.println(F("Mereni spusteno"));
		playStartMeasurementTone();
	} else if (deviceMode == DeviceMode::Paused) {
		resetMeasurementValues();
		Serial.println(F("Mereni pozastaveno"));
		playPauseMeasurementTone();
	} else {
		resetMeasurementValues();
		Serial.println(F("Pripraveno"));
	}

	refreshDisplay();
	updateBacklight();
	}

void updateButtons(unsigned long now) {
	const bool startReading = digitalRead(START_BUTTON_PIN);
	const bool stopReading = digitalRead(STOP_BUTTON_PIN);

	if (startReading != lastStartButtonReading) {
		lastStartButtonChangeMs = now;
		lastStartButtonReading = startReading;
	}
	if (stopReading != lastStopButtonReading) {
		lastStopButtonChangeMs = now;
		lastStopButtonReading = stopReading;
	}

	if (now - lastStartButtonChangeMs >= BUTTON_DEBOUNCE_MS && startReading != startButtonState) {
		startButtonState = startReading;
		if (startButtonState == LOW) {
			setDeviceMode(DeviceMode::Measuring);
		}
	}

	if (now - lastStopButtonChangeMs >= BUTTON_DEBOUNCE_MS && stopReading != stopButtonState) {
		stopButtonState = stopReading;
		if (stopButtonState == LOW) {
			setDeviceMode(DeviceMode::Paused);
		}
	}
	}

void updateVerdict(unsigned long now) {
	if (deviceMode != DeviceMode::Measuring) {
		verdict = Verdict::Calibrating;
		return;
	}

	if (gsrValue > 0) {
		baselineGsrSum += gsrValue;
		++baselineGsrSamples;
	}
	if (heartRate > 0) {
		baselineHeartRateSum += heartRate;
		++baselineHeartRateSamples;
	}

	if (measurementStartedMs == 0 || now - measurementStartedMs < BASELINE_CAPTURE_MS) {
		verdict = Verdict::Calibrating;
		return;
	}

	if (baselineGsr < 0 && baselineGsrSamples > 0) {
		baselineGsr = static_cast<int>(baselineGsrSum / baselineGsrSamples);
	}
	if (baselineHeartRate < 0 && baselineHeartRateSamples > 0) {
		baselineHeartRate = static_cast<int>(baselineHeartRateSum / baselineHeartRateSamples);
	}

	const int gsrDelta = baselineGsr >= 0 ? gsrValue - baselineGsr : 0;
	const int heartRateDelta = (baselineHeartRate >= 0 && heartRate > 0) ? heartRate - baselineHeartRate : 0;
	const bool stressDetected = gsrDelta >= GSR_LIE_DELTA || heartRateDelta >= HR_LIE_DELTA_BPM;
	verdict = stressDetected ? Verdict::Lie : Verdict::Truth;
	}

void updateEcg(unsigned long now) {
	ecgLeadsOff = isEcgLeadOff();
	if (ecgLeadsOff) {
		resetEcgTracking();
		return;
	}

	const int rawValue = analogRead(ECG_PIN);
	ecgValue = rawValue;

	if (!ecgTrackingReady) {
		ecgFiltered = rawValue;
		ecgBaseline = rawValue;
		ecgPreviousFiltered = rawValue;
		ecgTrackingReady = true;
		return;
	}

	ecgFiltered = (ecgFiltered * 3 + rawValue) / 4;
	ecgBaseline = (ecgBaseline * 31 + ecgFiltered) / 32;

	const int signal = ecgFiltered - ecgBaseline;
	const int absoluteSignal = abs(signal);
	if (absoluteSignal > ecgEnvelope) {
		ecgEnvelope = absoluteSignal;
	} else {
		ecgEnvelope = (ecgEnvelope * 15 + absoluteSignal) / 16;
	}

	int detectionThreshold = ecgEnvelope / 2;
	if (detectionThreshold < 12) {
		detectionThreshold = 12;
	}

	const int delta = ecgFiltered - ecgPreviousFiltered;
	if (!ecgPeakArmed && signal > detectionThreshold) {
		ecgPeakArmed = true;
	}

	const bool fallingEdgePeak = ecgPeakArmed && ecgPreviousDelta > 0 && delta <= 0 && signal > detectionThreshold;
	if (fallingEdgePeak) {
		const unsigned long beatIntervalMs = now - lastEcgBeatMs;
		if (lastEcgBeatMs == 0 || beatIntervalMs >= ECG_MIN_BEAT_INTERVAL_MS) {
			if (lastEcgBeatMs != 0 && beatIntervalMs <= ECG_MAX_BEAT_INTERVAL_MS) {
				ecgHeartRate = static_cast<int>(60000UL / beatIntervalMs);
				lastValidEcgBeatMs = now;
			}
			lastEcgBeatMs = now;
			startBeatBeep(now);
		}
		ecgPeakArmed = false;
	}

	if (now - lastValidEcgBeatMs > ECG_SIGNAL_TIMEOUT_MS) {
		ecgHeartRate = -1;
	}

	ecgPreviousFiltered = ecgFiltered;
	ecgPreviousDelta = delta;
	syncHeartRateSource();
	}

bool initLcd() {
	if (!probeI2C(LCD_TEXT_ADDR)) {
		return false;
	}

	if (probeI2C(LCD_BACKLIGHT_ADDR_PRIMARY)) {
		lcdBacklightAddress = LCD_BACKLIGHT_ADDR_PRIMARY;
	} else if (probeI2C(LCD_BACKLIGHT_ADDR_ALT)) {
		lcdBacklightAddress = LCD_BACKLIGHT_ADDR_ALT;
	} else {
		return false;
	}

	if (!lcd.begin(LCD_COLS, LCD_ROWS, LCD_TEXT_ADDR, lcdBacklightAddress)) {
		return false;
	}

	lcd.clear();
	lcd.setRGB(MENU_BACKLIGHT_RED, MENU_BACKLIGHT_GREEN, MENU_BACKLIGHT_BLUE);
	showStatusScreen("Pripraveno");
	return true;
	}

bool initOximeter() {
	if (!probeI2C(OXIMETER_ADDR)) {
		return false;
	}
	if (!oximeter.begin()) {
		return false;
	}
	oximeter.sensorStartCollect();
	return true;
	}

void updateBacklight() {
	if (!lcdReady) {
		return;
	}
	if (deviceMode == DeviceMode::Standby) {
		lcd.setRGB(MENU_BACKLIGHT_RED, MENU_BACKLIGHT_GREEN, MENU_BACKLIGHT_BLUE);
		return;
	}
	if (deviceMode == DeviceMode::Paused) {
		lcd.setRGB(PAUSED_BACKLIGHT_RED, PAUSED_BACKLIGHT_GREEN, PAUSED_BACKLIGHT_BLUE);
		return;
	}
	if (verdict == Verdict::Calibrating) {
		lcd.setRGB(180, 120, 0);
		return;
	}
	if (verdict == Verdict::Lie) {
		lcd.setRGB(255, 0, 0);
		return;
	}
	if (verdict == Verdict::Truth) {
		lcd.setRGB(0, 180, 0);
		return;
	}
	if (!oximeterReady) {
		lcd.setRGB(180, 20, 0);
		return;
	}
	if (spo2 >= 95 && heartRate > 0) {
		lcd.setRGB(0, 120, 20);
	} else if (spo2 > 0 || heartRate > 0) {
		lcd.setRGB(180, 80, 0);
	} else {
		lcd.setRGB(120, 80, 0);
	}
	}

void formatMaybeInt(char *buffer, size_t size, int value) {
	if (value < 0) {
		snprintf(buffer, size, "--");
		return;
	}
	snprintf(buffer, size, "%d", value);
	}

void refreshDisplay() {
	if (!lcdReady) {
		return;
	}
	if (deviceMode == DeviceMode::Standby) {
		showStatusScreen("Pripraveno");
		return;
	}
	if (deviceMode == DeviceMode::Paused) {
		showStatusScreen("Pozastaveno");
		return;
	}

	char hrBuffer[6];
	char ecgBuffer[6];
	char spo2Buffer[6];
	char tempBuffer[6];
	char verdictBuffer[7];
	char line1[17];
	char line2[17];

	formatMaybeInt(hrBuffer, sizeof(hrBuffer), heartRate);
	if (ecgLeadsOff) {
		snprintf(ecgBuffer, sizeof(ecgBuffer), "OFF");
	} else {
		snprintf(ecgBuffer, sizeof(ecgBuffer), "%d", ecgValue);
	}
	formatMaybeInt(spo2Buffer, sizeof(spo2Buffer), spo2);
	formatMaybeInt(tempBuffer, sizeof(tempBuffer), boardTempC);
	if (verdict == Verdict::Calibrating) {
		snprintf(verdictBuffer, sizeof(verdictBuffer), "KALIB");
	} else if (verdict == Verdict::Lie) {
		snprintf(verdictBuffer, sizeof(verdictBuffer), "LEZ");
	} else {
		snprintf(verdictBuffer, sizeof(verdictBuffer), "PRAVDA");
	}

	snprintf(line1, sizeof(line1), "G%4d H%3s E%4s", gsrValue, hrBuffer, ecgBuffer);
	snprintf(line2, sizeof(line2), "%s O2%s T%s", verdictBuffer, spo2Buffer, tempBuffer);

	lcd.setCursor(0, 0);
	for (uint8_t i = 0; i < LCD_COLS; ++i) {
		lcd.write(i < strlen(line1) ? line1[i] : ' ');
	}
	lcd.setCursor(0, 1);
	for (uint8_t i = 0; i < LCD_COLS; ++i) {
		lcd.write(i < strlen(line2) ? line2[i] : ' ');
	}
	}

void printStatusToSerial() {
	Serial.print(F("Mode="));
	if (deviceMode == DeviceMode::Standby) {
		Serial.print(F("READY"));
	} else if (deviceMode == DeviceMode::Paused) {
		Serial.print(F("PAUSED"));
	} else {
		Serial.print(F("MEASURE"));
	}
	Serial.print(F(" | "));
	Serial.print(F("GSR="));
	Serial.print(gsrValue);
	Serial.print(F(" | ECG="));
	if (ecgLeadsOff) {
		Serial.print(F("LEADS_OFF"));
	} else {
		Serial.print(ecgValue);
	}
	Serial.print(F(" | HR="));
	if (heartRate >= 0) {
		Serial.print(heartRate);
		Serial.print(F(" bpm"));
	} else {
		Serial.print(F("--"));
	}
	Serial.print(F(" | HR_OX="));
	if (oximeterHeartRate >= 0) {
		Serial.print(oximeterHeartRate);
		Serial.print(F(" bpm"));
	} else {
		Serial.print(F("--"));
	}
	Serial.print(F(" | SpO2="));
	if (spo2 >= 0) {
		Serial.print(spo2);
		Serial.print('%');
	} else {
		Serial.print(F("--"));
	}
	Serial.print(F(" | Temp="));
	if (boardTempC >= 0) {
		Serial.print(boardTempC);
		Serial.print(F(" C"));
	} else {
		Serial.print(F("--"));
	}
	Serial.print(F(" | LCD="));
	Serial.print(lcdReady ? F("OK") : F("ERR"));
	Serial.print(F(" | OX="));
	Serial.println(oximeterReady ? F("OK") : F("WAIT"));
	Serial.print(F("Verdict="));
	if (verdict == Verdict::Calibrating) {
		Serial.println(F("CALIBRATING"));
	} else if (verdict == Verdict::Lie) {
		Serial.println(F("LIE"));
	} else {
		Serial.println(F("TRUTH"));
	}
	Serial.println(F("Cmd: s=scan, c=clear, b=buzzer test"));
	}

void updateOximeter() {
	if (!oximeterReady) {
		return;
	}
	oximeter.getHeartbeatSPO2();
	oximeterHeartRate = oximeter._sHeartbeatSPO2.Heartbeat;
	spo2 = oximeter._sHeartbeatSPO2.SPO2;
	boardTempC = static_cast<int>(oximeter.getTemperature_C() + 0.5f);
	syncHeartRateSource();
	}
} // namespace

void setup() {
	Serial.begin(115200);
	const unsigned long serialWaitStart = millis();
	while (!Serial && millis() - serialWaitStart < 3000) {
	}

	pinMode(GSR_PIN, INPUT);
	pinMode(ECG_PIN, INPUT);
	pinMode(ECG_LO_MINUS_PIN, INPUT);
	pinMode(ECG_LO_PLUS_PIN, INPUT);
	pinMode(BUZZER_PIN, OUTPUT);
	pinMode(START_BUTTON_PIN, INPUT_PULLUP);
	pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
	noTone(BUZZER_PIN);
	Wire.begin();
	Wire.setClock(100000);

	Serial.println();
	Serial.println(F("Detektor LZI"));
	Serial.println(F("UNO: SDA=A4, SCL=A5, GSR=A0, AD8232=A1 D2 D3, BUZ=D5, BTN=D8 D9"));
	scanI2CBus();

	lcdReady = initLcd();
	oximeterReady = initOximeter();
	playStartupMelody();
	setDeviceMode(DeviceMode::Standby);

	if (lcdReady) {
		refreshDisplay();
	}
	updateBacklight();
	printStatusToSerial();

	const unsigned long now = millis();
	lastGsrSampleMs = now;
	lastEcgSampleMs = now;
	lastDisplayRefreshMs = now;
	lastSerialPrintMs = now;
	lastOximeterReadMs = now - OXIMETER_READ_INTERVAL_MS;
	lastOximeterRetryMs = now;
	}

void loop() {
	const unsigned long now = millis();
	updateButtons(now);

	if (deviceMode == DeviceMode::Measuring && now - lastGsrSampleMs >= GSR_SAMPLE_INTERVAL_MS) {
		gsrValue = sampleGsr();
		updateVerdict(now);
		lastGsrSampleMs = now;
	}

	if (deviceMode == DeviceMode::Measuring && now - lastEcgSampleMs >= ECG_SAMPLE_INTERVAL_MS) {
		updateEcg(now);
		lastEcgSampleMs = now;
	}

	if (deviceMode == DeviceMode::Measuring && !oximeterReady && now - lastOximeterRetryMs >= OXIMETER_RETRY_INTERVAL_MS) {
		oximeterReady = initOximeter();
		lastOximeterRetryMs = now;
	}

	if (deviceMode == DeviceMode::Measuring && oximeterReady && now - lastOximeterReadMs >= OXIMETER_READ_INTERVAL_MS) {
		updateOximeter();
		updateVerdict(now);
		lastOximeterReadMs = now;
	}

	if (now - lastDisplayRefreshMs >= DISPLAY_REFRESH_INTERVAL_MS) {
		refreshDisplay();
		updateBacklight();
		lastDisplayRefreshMs = now;
	}

	if (now - lastSerialPrintMs >= SERIAL_PRINT_INTERVAL_MS) {
		printStatusToSerial();
		lastSerialPrintMs = now;
	}

	updateBuzzer(now);

	if (Serial.available() > 0) {
		const char command = static_cast<char>(Serial.read());
		if (command == 's' || command == 'S') {
			scanI2CBus();
		} else if (command == 'c' || command == 'C') {
			if (lcdReady) {
				lcd.clear();
			}
		} else if (command == 'b' || command == 'B') {
			triggerBuzzerTest(now);
			Serial.println(F("Buzzer test"));
		}
	}
	}
