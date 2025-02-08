#include "LilyGo_TWR.h"
#include "../Constants.h"
#include <AceButton.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>

#define BLE_RECEIVED_FILE "/received_image.bin"
#define CHANNEL A0
#define BIT_DURATION 250  // Adjust if needed
#define REF_VOLTAGE 700  // Midpoint of ADC range - from serial plotter testing around 700

/**
 * Findings:
 *  voltage during rest around 700,
 *  a lot of noise
 *  hard to do proper sampling rate, as running the loop itself also takes time
 * 
 * TODOs:
 *  zero cross detection implemenation
 *  running average calculation
 *  error correction?
 *  filters?
 */

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

    File file = SD.open(BLE_RECEIVED_FILE, FILE_WRITE);
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

    // Initialize TWR
    twr.begin();

    // Initialize SD card
    initializeSDCard();

    // Initialize OLED
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

    // Initialize buttons
    for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        buttons[i].init(buttonPins[i], HIGH, i);
    }

    // Set radio to receive mode
    radio.receive();
}

void loop() {
    int signalValue = analogRead(CHANNEL);

    //Serial.println(analogRead(CHANNEL));  //testing purpose
    //delay(10);  // Small delay for better visualization


    unsigned long currentTime = micros();

    // Detect zero crossing (sign change)
    //right now very crude way, with +-10, but should do it properly with some formula
    if ((lastSignal < REF_VOLTAGE-10 && signalValue >= REF_VOLTAGE+10) || 
        (lastSignal > REF_VOLTAGE+10 && signalValue <= REF_VOLTAGE-10)) {
        
        // Measure time difference
        unsigned long period = currentTime - lastZeroCrossing;
        lastZeroCrossing = currentTime;

        // Convert period to frequency
        float frequency = 1e6 / (float)period;  // Convert microseconds to Hz

        Serial.println(frequency);
        
        if (frequency < 1500) {  // Adjust threshold based on transmitter settings
            receivedBits += "0";
            Serial.println("Received bit: 0");
        } else {
            receivedBits += "1";
            Serial.println("Received bit: 1");
        }

        if (receivedBits.length() >= 8) {  // Save every 8 bits (1 byte)
            saveDataToFile(receivedBits);
            receivedBits = "";
        }
    }

    lastSignal = signalValue;

    // Check buttons
    for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); i++) {
        buttons[i].check();
    }
}
