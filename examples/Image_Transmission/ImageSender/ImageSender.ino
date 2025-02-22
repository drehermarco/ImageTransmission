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

// Audio libraries for WAV playback (no longer used for text, but left here if needed)
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceSD.h>
#include <AudioOutputI2S.h>
#include <cstring>  // For strlen()


#define BIT_DURATION 100 // in ms
#define FREQUENCY_0 600 // in Hz
#define FREQUENCY_1 1200 // in Hz

//#define SYMBOL_DURATION (BIT_DURATION*2)

// Define I2S_PIN_NO_CHANGE if not already defined.
#ifndef I2S_PIN_NO_CHANGE
  #define I2S_PIN_NO_CHANGE (-1)
#endif

// --- BLE Definitions ---
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_DEVICE_NAME         "T-TWR"
#define BLE_RECEIVED_FILE       "/received.txt"   // SD file where the received text data is saved

#define FREQUENCY_0  600
#define FREQUENCY_1  1200
// --- OLED Display Setup ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- AceButton Setup ---
using namespace ace_button;
AceButton                           buttons[3];

const uint8_t buttonPins[] = {
    ENCODER_OK_PIN,    // Transmit/Receive button (assumed ID 1)
    BUTTON_PTT_PIN,
    BUTTON_DOWN_PIN
};

enum ButtonType {
    ShortPress,
    LongPress,
    Unknown
};

// --- Global File Handle for BLE Reception ---
File textFile;

// --- BLE Server Callbacks ---
class MyBLEServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    Serial.println("BLE client connected, opening text file for writing.");
    textFile = SD.open(BLE_RECEIVED_FILE, FILE_WRITE);
    if (!textFile) {
      Serial.println("Error opening text file for writing!");
    }
  }
  
  void onDisconnect(BLEServer* pServer) override {
    Serial.println("BLE client disconnected, closing text file.");
    if (textFile) {
      textFile.close();
    }
    pServer->getAdvertising()->start();  // Restart advertising for new clients
  }
};

// --- BLE Characteristic Callbacks ---
class MyBLECharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string rxValue = pCharacteristic->getValue();
    if (!rxValue.empty()) {
      Serial.print("Received ");
      Serial.print(rxValue.size());
      Serial.println(" bytes via BLE");
      if (textFile) {
        textFile.write((const uint8_t*)rxValue.data(), rxValue.size());
        textFile.flush();
      } else {
        Serial.println("Text file not open for writing!");
      }
    }
  }
};

// --- BLE Setup ---
void setupBLE() {
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyBLEServerCallbacks());
    
    BLEService *pService = pServer->createService(BLE_SERVICE_UUID);
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    pCharacteristic->setCallbacks(new MyBLECharacteristicCallbacks());
    pService->start();
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE setup complete. Waiting for text data...");
}

// --- SD Card Initialization ---
void initializeSDCard() {
    Serial.println("Initializing SD card...");
    if (!SD.begin()) {
        Serial.println("Failed to initialize SD card.");
        return;
    }
    Serial.println("SD card initialized successfully.");
    if (SD.exists(BLE_RECEIVED_FILE)) {
        SD.remove(BLE_RECEIVED_FILE);
    }

    
    File file = SD.open(BLE_RECEIVED_FILE, FILE_WRITE);
    file.println("0101010101010111111110101010101010101010010110101101010101010101010010111111010101101010101010001101010100001010010101000000011110100100010110101010000110000101001010000101000000100100100010010010010010010100001000100100100010100100100010001001001010100000100000000001010010001011010000000100000010100010011000010010000010100100000011110000000001000000101001011111010000000001001000000001011100010010001000000101001111110000000010010100000010001111001001000100000100100010111100000001000000000000010101111000000000010000100101010111000010010000010000010111111001000000010000010000101011111000000000010000011101011101010000100100010101011011110000000000000000000101011110010101000000010101101111100100010010000000001011101010000000100000100000000101010010101001000000000001000101111111");
    Serial.println("SD card ready for file storage.");
}


// --- Signal Generation ---
// Generates a tone based on the bit pair read from data string.
void generateSignal(uint8_t pin, uint8_t channel, String data) {

    ledcAttachPin(pin, channel);
    char bit;
    for (size_t i = 0; i < data.length(); i++) {
        bit = data.charAt(i);
        
        if (bit == '0') {
            ledcWriteTone(channel, FREQUENCY_0);
        } else if (bit == '1') {
            ledcWriteTone(channel, FREQUENCY_1);
        } 
        delay(BIT_DURATION);
    }
    ledcWriteTone(channel,0);
    ledcDetachPin(pin);
}



void sendImageData(uint8_t pin, uint8_t channel) {
    File file = SD.open(BLE_RECEIVED_FILE, FILE_READ);

    String data = file.readString();
    file.close();
    SD.remove(BLE_RECEIVED_FILE);
    generateSignal(pin, channel, data);
    
}

void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState) {
    uint8_t id = button->getId();
    if (id == 1) { // Check if the correct button is pressed
        switch (eventType) {
        case AceButton::kEventPressed:
            radio.transmit(); // Start transmission
            sendImageData(ESP2SA868_MIC, 0); // Send the image binary data
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

// --- Setup Function ---
void setup() {
    bool rlst = false;
    Serial.begin(115200);
    twr.begin();
    
    if (twr.getVersion() == TWRClass::TWR_REV2V1) {
        Serial.println("Detection using TWR Rev2.1");
        radio.setPins(SA868_PTT_PIN, SA868_PD_PIN);
        rlst = radio.begin(RadioSerial, twr.getBandDefinition());
    } else {
        Serial.println("Detection using TWR Rev2.0");
        radio.setPins(SA868_PTT_PIN, SA868_PD_PIN, SA868_RF_PIN);
        rlst = radio.begin(RadioSerial, SA8X8_VHF);
    }
    
    if (!rlst) {
        while (1) {
            Serial.println("SA868 is not online!");
            delay(1000);
        }
    }
    
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
    
    uint8_t addr = twr.getOLEDAddress();
    while (addr == 0xFF) {
        Serial.println("OLED is not detected, please confirm whether OLED is installed normally.");
        delay(1000);
    }
    u8g2.setI2CAddress(addr << 1);
    u8g2.begin();
    u8g2.setFontMode(0);
    u8g2.setFont(u8g2_font_cu12_hr);
    u8g2.setCursor(0,20);
    u8g2.print("Image Sender");
    u8g2.sendBuffer();
    
    radio.setRxFreq(446200000);
    radio.setTxFreq(446200000);
    radio.setRxCXCSS(0);
    radio.setTxCXCSS(0);
    
    initializeSDCard();
    setupBLE();
    twr.routingSpeakerChannel(TWRClass::TWR_ESP_TO_SPK);
}

// --- Main Loop ---
void loop() {
    for (uint8_t i = 0; i < COUNT(buttonPins); i++) {
        buttons[i].check();
    }
}
