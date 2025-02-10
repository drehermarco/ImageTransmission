/**
 * @file      SA868_ESPSendAudio_Example.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-01-05
 *
 */

#include "LilyGo_TWR.h"
#include "../Constants.h"
#include <AceButton.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_DEVICE_NAME         "T-TWR"
#define BLE_RECEIVED_FILE       "/received_data.txt"

#define BIT_DURATION 100

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


using namespace ace_button;
AceButton                           buttons[3];


const uint8_t                       buttonPins [] = {
    ENCODER_OK_PIN,
    BUTTON_PTT_PIN,
    BUTTON_DOWN_PIN
};

enum Button {
    ShortPress,
    LongPress,
    Unknown
};

String receivedData = ""; // Buffer for incoming BLE data


void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState) {
    uint8_t id = button->getId();
    if (id == 1) { // Check if the correct button is pressed
        switch (eventType) {
        case AceButton::kEventPressed:
            radio.transmit(); // Start transmission
            sendImageBinary(ESP2SA868_MIC, 0); // Send the image binary data
            radio.receive(); // Switch back to receiving mode
            break;
        case AceButton::kEventReleased:
            radio.receive();
            break;
        default:
            break;
        }
    }
}

void initializeSDCard() {
    Serial.println("Initializing SD card...");

    if (!SD.begin()) {
        Serial.println("Failed to initialize SD card.");
        return;
    }

    Serial.println("SD card initialized successfully.");

    File file = SD.open(BLE_RECEIVED_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing.");
        return;
    }

    file.println("11110000");  //Always starts the device with test data for ease of testing
    file.close();
    
    Serial.println("Test data written to SD card.");
}

void playMessage(uint8_t pin, uint8_t channel, const char* binaryData) {
    ledcAttachPin(pin, channel);
    for (size_t i = 0; binaryData[i] != '\0'; i++) {
        if (binaryData[i] == '0') {
            ledcWriteTone(channel, 1000);
        } else if (binaryData[i] == '1') { //So it doesn't play addiotional 1 at the end for newline
            ledcWriteTone(channel, 2000);
        }
        delay(BIT_DURATION); //Play each sounds for bit duration
    }
    ledcDetachPin(pin);
}

void sendImageBinary(uint8_t pin, uint8_t channel) {
    if (!SD.begin()) {
        Serial.println("Failed to initialize SD card.");
        return;
    }

    File file = SD.open(BLE_RECEIVED_FILE);
    if (!file) {
        Serial.println("Failed to open binary file.");
        return;
    }

    Serial.println("Transmitting image data...");
    while (file.available()) {
        char buffer[128];
        size_t bytesRead = file.readBytes(buffer, sizeof(buffer) - 1);
        buffer[bytesRead] = '\0'; // Null-terminate the buffer
        playMessage(pin, channel, buffer);
    }

    file.close();
    Serial.println("Image data transmission complete.");
}

// BLE Callback to handle received data
class BLECallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        receivedData += pCharacteristic->getValue().c_str();
        Serial.println("Data chunk received: " + receivedData);
        if (receivedData.endsWith("\n") || receivedData.length() > 256) { // Arbitrary buffer size limit
            saveDataToFile(receivedData);
            receivedData = ""; // Clear buffer
        }
    }

    void saveDataToFile(const String &data) {
        File file = SD.open(BLE_RECEIVED_FILE, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open file for writing.");
            return;
        }
        file.print(data); //New data received via bluetooth overwrites the old data
        file.close();
        Serial.println("Data saved to file.");
    }
};

void setupBLE() {
    BLEDevice::init(BLE_DEVICE_NAME);  // Set the BLE device name
    BLEServer *pServer = BLEDevice::createServer();  // Create BLE server
    BLEService *pService = pServer->createService(BLE_SERVICE_UUID);  // Create BLE service

    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

    pCharacteristic->setCallbacks(new BLECallbacks());
    pService->start();  // Start BLE service

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    BLEDevice::startAdvertising();

    Serial.println("BLE setup complete. Waiting for data...");
}

void setup() {
    bool rlst = false;

    Serial.begin(115200);

    twr.begin();

    if (twr.getVersion() == TWRClass::TWR_REV2V1) {
        Serial.println("Detection using TWR Rev2.1");

        // Initialize SA868
        radio.setPins(SA868_PTT_PIN, SA868_PD_PIN);
        rlst = radio.begin(RadioSerial, twr.getBandDefinition());

    } else {

        Serial.println("Detection using TWR Rev2.0");

        //* Designated as UHF
        //  rlst = radio.begin(RadioSerial, SA8X8_UHF);

        //* Designated as VHF
        radio.setPins(SA868_PTT_PIN, SA868_PD_PIN, SA868_RF_PIN);
        rlst = radio.begin(RadioSerial, SA8X8_VHF);

    }

    if (!rlst) {
        while (1) {
            Serial.println("SA868 is not online !");
            delay(1000);
        }
    }

    // Initialize the buttons
    for (uint8_t i = 0; i < COUNT(buttonPins); i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        buttons[i].init(buttonPins[i], HIGH, i);
    }
    ButtonConfig *buttonConfig = ButtonConfig::getSystemButtonConfig();
    buttonConfig->setEventHandler(handleEvent);
    buttonConfig->setFeature(ButtonConfig::kFeatureClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
    buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);

    //* Microphone will be routed to ESP ADC IO15 and the SA868 audio input will be routed to ESP IO18
    twr.routingMicrophoneChannel(TWRClass::TWR_MIC_TO_ESP);

    // Start transfer
    // radio.transmit();
    twr.enablePowerOff(true);

    // Setup for OLED
    uint8_t addr = twr.getOLEDAddress();
    while (addr == 0xFF) {
        Serial.println("OLED is not detected, please confirm whether OLED is installed normally.");
        delay(1000);
    }
    u8g2.setI2CAddress(addr << 1);
    u8g2.begin();
    u8g2.setFontMode(0);               // write solid glyphs
    u8g2.setFont(u8g2_font_cu12_hr);   // choose a suitable h font
    u8g2.setCursor(0,20);              // set write position
    u8g2.print("SENDER");              // use extra spaces here
    u8g2.sendBuffer();                 // transfer internal memory to the display
    radio.setRxFreq(446200000);
    radio.setTxFreq(446200000);
    radio.setRxCXCSS(0);
    radio.setTxCXCSS(0);

    initializeSDCard();

    setupBLE(); // Initialize BLE
}

void loop() {
    for (uint8_t i = 0; i < COUNT(buttonPins); i++) {
        buttons[i].check();
    }
}
