#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_BloodOxygen_S.h>

namespace {
constexpr uint8_t GSR_PIN = A0;
constexpr uint8_t LCD_TEXT_ADDR = 0x3E;
constexpr uint8_t LCD_BACKLIGHT_ADDR_PRIMARY = 0x62;
constexpr uint8_t LCD_BACKLIGHT_ADDR_ALT = 0x60;
constexpr uint8_t OXIMETER_ADDR = 0x57;

constexpr uint8_t LCD_COLS = 16;
constexpr uint8_t LCD_ROWS = 2;

constexpr unsigned long GSR_SAMPLE_INTERVAL_MS = 250;
constexpr unsigned long DISPLAY_REFRESH_INTERVAL_MS = 500;
constexpr unsigned long SERIAL_PRINT_INTERVAL_MS = 1000;
constexpr unsigned long OXIMETER_READ_INTERVAL_MS = 4000;
constexpr unsigned long OXIMETER_RETRY_INTERVAL_MS = 5000;

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

RgbLcd lcd;
DFRobot_BloodOxygen_S_I2C oximeter(&Wire, OXIMETER_ADDR);

bool lcdReady = false;
bool oximeterReady = false;
uint8_t lcdBacklightAddress = LCD_BACKLIGHT_ADDR_PRIMARY;

int gsrValue = 0;
int heartRate = -1;
int spo2 = -1;
int boardTempC = -1;

unsigned long lastGsrSampleMs = 0;
unsigned long lastDisplayRefreshMs = 0;
unsigned long lastSerialPrintMs = 0;
unsigned long lastOximeterReadMs = 0;
unsigned long lastOximeterRetryMs = 0;

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

int sampleGsr() {
	long total = 0;
	for (uint8_t index = 0; index < 8; ++index) {
		total += analogRead(GSR_PIN);
		delayMicroseconds(300);
	}
	return static_cast<int>(total / 8);
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
	lcd.setRGB(180, 80, 0);
	lcd.setCursor(0, 0);
	lcd.print(F("Detektor LZI"));
	lcd.setCursor(0, 1);
	lcd.print(F("Startuji..."));
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

	char hrBuffer[6];
	char spo2Buffer[6];
	char tempBuffer[6];
	char line1[17];
	char line2[17];

	formatMaybeInt(hrBuffer, sizeof(hrBuffer), heartRate);
	formatMaybeInt(spo2Buffer, sizeof(spo2Buffer), spo2);
	formatMaybeInt(tempBuffer, sizeof(tempBuffer), boardTempC);

	snprintf(line1, sizeof(line1), "GSR %4d HR %3s", gsrValue, hrBuffer);
	snprintf(line2, sizeof(line2), "O2 %3s%% T%2sC", spo2Buffer, tempBuffer);

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
	Serial.print(F("GSR="));
	Serial.print(gsrValue);
	Serial.print(F(" | HR="));
	if (heartRate >= 0) {
		Serial.print(heartRate);
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
	}

void updateOximeter() {
	if (!oximeterReady) {
		return;
	}
	oximeter.getHeartbeatSPO2();
	heartRate = oximeter._sHeartbeatSPO2.Heartbeat;
	spo2 = oximeter._sHeartbeatSPO2.SPO2;
	boardTempC = static_cast<int>(oximeter.getTemperature_C() + 0.5f);
	}
} // namespace

void setup() {
	Serial.begin(115200);
	const unsigned long serialWaitStart = millis();
	while (!Serial && millis() - serialWaitStart < 3000) {
	}

	pinMode(GSR_PIN, INPUT);
	Wire.begin();
	Wire.setClock(100000);

	Serial.println();
	Serial.println(F("Detektor LZI"));
	Serial.println(F("UNO: SDA=A4, SCL=A5, GSR=A0"));
	scanI2CBus();

	lcdReady = initLcd();
	oximeterReady = initOximeter();
	gsrValue = sampleGsr();

	if (lcdReady) {
		refreshDisplay();
	}
	updateBacklight();
	printStatusToSerial();

	const unsigned long now = millis();
	lastGsrSampleMs = now;
	lastDisplayRefreshMs = now;
	lastSerialPrintMs = now;
	lastOximeterReadMs = now - OXIMETER_READ_INTERVAL_MS;
	lastOximeterRetryMs = now;
	}

void loop() {
	const unsigned long now = millis();

	if (now - lastGsrSampleMs >= GSR_SAMPLE_INTERVAL_MS) {
		gsrValue = sampleGsr();
		lastGsrSampleMs = now;
	}

	if (!oximeterReady && now - lastOximeterRetryMs >= OXIMETER_RETRY_INTERVAL_MS) {
		oximeterReady = initOximeter();
		lastOximeterRetryMs = now;
	}

	if (oximeterReady && now - lastOximeterReadMs >= OXIMETER_READ_INTERVAL_MS) {
		updateOximeter();
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

	if (Serial.available() > 0) {
		const char command = static_cast<char>(Serial.read());
		if (command == 's' || command == 'S') {
			scanI2CBus();
		} else if (command == 'c' || command == 'C') {
			if (lcdReady) {
				lcd.clear();
			}
		}
	}
	}
