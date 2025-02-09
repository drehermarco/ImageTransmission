#include "LilyGo_TWR.h"
#include "../Constants.h"
#include <AceButton.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>

#define RECEIVED_FILE "/received_image.bin"
#define CHANNEL A0
#define BIT_DURATION 250000  // 250ms in microseconds
#define AVG_WINDOW 10  // Number of samples for running average
#define LOW_FREQ_THRESHOLD 900  // Hz threshold for low frequency (adjustable)
#define HIGH_FREQ_THRESHOLD 1800  // Hz threshold for high frequency (adjustable)

using namespace ace_button;
AceButton buttons[3];

const uint8_t buttonPins[] = {
    ENCODER_OK_PIN,
    BUTTON_PTT_PIN,
    BUTTON_DOWN_PIN
};

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

bool sdInitialized = false;
String receivedBits = "";

unsigned long lastZeroCrossing = 0;
int lastSignal = 0;
float runningAvg = 700;
int avgBuffer[AVG_WINDOW] = {700};
int avgIndex = 0;
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL = 100;  // Adjust for desired sampling rate (in microseconds)
unsigned long zeroCrossings = 0;
unsigned long bitMeasureStart = 0;

void initializeSDCard() {
    if (!SD.begin(SD_CS, SPI)) {
        Serial.println("Failed to initialize SD card.");
        return;
    }
    sdInitialized = true;
    Serial.println("SD card initialized successfully.");
}

void saveDataToFile(const String &data) {
    if (!sdInitialized) {
        Serial.println("SD card not initialized.");
        return;
    }

    File file = SD.open(RECEIVED_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing.");
        return;
    }

    file.print(data);
    file.close();
    Serial.println("Data saved: " + data);
}

void setup() {
    Serial.begin(115200);
    twr.begin();
    initializeSDCard();
    
    uint8_t addr = twr.getOLEDAddress();
    while (addr == 0xFF) {
        Serial.println("OLED is not detected, check installation.");
        delay(1000);
    }
    u8g2.setI2CAddress(addr << 1);
    u8g2.begin();
    u8g2.setFontMode(0);
    u8g2.setFont(u8g2_font_cu12_hr);
    u8g2.setCursor(0, 20);
    u8g2.print("RECEIVER");
    u8g2.sendBuffer();

    for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        buttons[i].init(buttonPins[i], HIGH, i);
    }
    radio.receive();
    bitMeasureStart = micros();
}

void loop() {
    unsigned long currentTime = micros();
    if (currentTime - lastSampleTime < SAMPLE_INTERVAL) {
        return; // Ensures we sample at a steady rate
    }
    lastSampleTime = currentTime;

    int signalValue = analogRead(CHANNEL);;
    // Update running average
    avgBuffer[avgIndex] = signalValue;
    avgIndex = (avgIndex + 1) % AVG_WINDOW;
    runningAvg = 0;
    for (int i = 0; i < AVG_WINDOW; i++) {
        runningAvg += avgBuffer[i];
    }
    runningAvg /= AVG_WINDOW;

    // Zero crossing detection - +5/-5, as the measurements are not perfect
    if ((lastSignal < runningAvg-5 && signalValue >= runningAvg+5) ||
        (lastSignal > runningAvg+5 && signalValue <= runningAvg+5)) {
        zeroCrossings++;
    }
    
    lastSignal = signalValue;

    if (currentTime - bitMeasureStart >= BIT_DURATION) {
        float frequency = (zeroCrossings / 2.0) / (BIT_DURATION / 1e6);
        Serial.println(frequency);
        
        if (frequency >= LOW_FREQ_THRESHOLD && frequency < HIGH_FREQ_THRESHOLD) {
            receivedBits += "0";
            Serial.println("Stored bit: 0");
        } else if (frequency >= HIGH_FREQ_THRESHOLD) {
            receivedBits += "1";
            Serial.println("Stored bit: 1");
        } else {
            Serial.println("No valid bit detected.");
        }

        if (receivedBits.length() >= 8) {
            saveDataToFile(receivedBits);
            receivedBits = "";
        }
        
        zeroCrossings = 0;
        bitMeasureStart = currentTime;
    }

    for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) {
        buttons[i].check();
    }
}
