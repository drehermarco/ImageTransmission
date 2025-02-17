# 1 "C:\\Users\\wyco\\AppData\\Local\\Temp\\tmpo1a2kkid"
#include <Arduino.h>
# 1 "C:/Uni/ImageTransmission/examples/Image_Transmission/newreceiver/Newreceiver.ino"
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
#define BIT_DURATION 100000
#define AVG_WINDOW 10
#define LOW_FREQ_THRESHOLD 900
#define HIGH_FREQ_THRESHOLD 1800

using namespace ace_button;
AceButton buttons[3];

const uint8_t buttonPins[] = {
    ENCODER_OK_PIN,
    BUTTON_PTT_PIN,
    BUTTON_DOWN_PIN
};

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

bool sdInitialized = false;
String receivedBits = "";

unsigned long lastZeroCrossing = 0;
int lastSignal = 0;
float runningAvg = 700;
int avgBuffer[AVG_WINDOW] = {700};
int avgIndex = 0;
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL = 100;
unsigned long zeroCrossings = 0;
unsigned long bitMeasureStart = 0;
void initializeSDCard();
void saveDataToFile(const String &data);
void setup();
void loop();
#line 41 "C:/Uni/ImageTransmission/examples/Image_Transmission/newreceiver/Newreceiver.ino"
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

    file.seek(file.size());
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
# 108 "C:/Uni/ImageTransmission/examples/Image_Transmission/newreceiver/Newreceiver.ino"
void loop() {
    unsigned long currentTime = micros();
    if (currentTime - lastSampleTime < SAMPLE_INTERVAL) {
        return;
    }
    lastSampleTime = currentTime;

    int signalValue = analogRead(CHANNEL);;

    avgBuffer[avgIndex] = signalValue;
    avgIndex = (avgIndex + 1) % AVG_WINDOW;
    runningAvg = 0;
    for (int i = 0; i < AVG_WINDOW; i++) {
        runningAvg += avgBuffer[i];
    }
    runningAvg /= AVG_WINDOW;


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