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

// Audio libraries for WAV playback
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceSD.h>
#include <AudioOutputI2S.h>

// Define I2S_PIN_NO_CHANGE if not already defined.
#ifndef I2S_PIN_NO_CHANGE
  #define I2S_PIN_NO_CHANGE (-1)
#endif

// --- BLE Definitions ---
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_DEVICE_NAME         "T-TWR"
#define BLE_RECEIVED_FILE       "/received.wav"   // SD file where the received WAV is saved

// --- OLED Display Setup ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- AceButton Setup ---
using namespace ace_button;
AceButton buttons[3];
const uint8_t buttonPins[] = {
    ENCODER_OK_PIN,    // Transmit/Receive button (assumed ID 1)
    BUTTON_PTT_PIN,
    BUTTON_DOWN_PIN
};

enum Button {
    ShortPress,
    LongPress,
    Unknown
};

// --- Global File Handle for BLE Reception ---
File wavFile;

// --- BLE Server Callbacks ---
class MyBLEServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    Serial.println("BLE client connected, opening WAV file for writing.");
    wavFile = SD.open(BLE_RECEIVED_FILE, FILE_WRITE);
    if (!wavFile) {
      Serial.println("Error opening WAV file for writing!");
    }
  }
  
  void onDisconnect(BLEServer* pServer) override {
    Serial.println("BLE client disconnected, closing WAV file.");
    if (wavFile) {
      wavFile.close();
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
      if (wavFile) {
        // Write the raw binary data to the WAV file on SD.
        wavFile.write((const uint8_t*)rxValue.data(), rxValue.size());
        wavFile.flush(); // Ensure data is written immediately.
      } else {
        Serial.println("WAV file not open for writing!");
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
    
    Serial.println("BLE setup complete. Waiting for WAV file data...");
}

// --- SD Card Initialization ---
void initializeSDCard() {
    Serial.println("Initializing SD card...");
    if (!SD.begin()) {
        Serial.println("Failed to initialize SD card.");
        return;
    }
    Serial.println("SD card initialized successfully.");
    // Remove any previous file so we start fresh.
    if (SD.exists(BLE_RECEIVED_FILE)) {
        SD.remove(BLE_RECEIVED_FILE);
    }
    Serial.println("SD card ready for file storage.");
}

// --- Function to Transmit WAV over Radio ---
// This function plays the stored WAV file using the AudioGeneratorWAV classes.
// It assumes that radio.transmit() was already called so that the I2S audio is routed to the RF front-end.
void sendWavOverRadio() {
    if (!SD.exists(BLE_RECEIVED_FILE)) {
        Serial.println("No WAV file found on SD.");
        return;
    }
    
    // Create the audio file source from SD.
    AudioFileSourceSD *file = new AudioFileSourceSD(BLE_RECEIVED_FILE);
    // Create the I2S output. INTERNAL_PDM is used for the built-in DAC.
    AudioOutputI2S *out = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
    // Configure the I2S output pins; using I2S_PIN_NO_CHANGE preserves default pin assignments.
    out->SetPinout(I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE, ESP32_PWM_TONE, I2S_PIN_NO_CHANGE);
    out->SetOutputModeMono(true);
    out->SetGain(1.0);
    out->SetRate(115200);
    
    // Create the WAV decoder.
    AudioGeneratorWAV *wav = new AudioGeneratorWAV();
    if (!wav->begin(file, out)) {
        Serial.println("WAV begin failed.");
        delete file;
        delete out;
        delete wav;
        return;
    }
    
    Serial.println("Transmitting WAV over radio...");
    // Play the WAV file until finished.
    while (wav->isRunning()) {
        if (!wav->loop()) break;
    }
    wav->stop();
    Serial.println("WAV transmission complete.");
    
    // Clean up the audio objects.
    delete wav;
    delete file;
    delete out;
}

// --- Button Event Handler ---
// When the Transmit/Receive button (assumed button ID 1) is pressed,
// the device switches to TX mode, sends the stored WAV file over radio, then reverts to RX mode.
void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState) {
    uint8_t id = button->getId();
    if (id == 1) {  // Transmit/Receive button
        if (eventType == AceButton::kEventPressed) {
            Serial.println("Transmit/Receive button pressed: starting transmission.");
            // Set the routing to radio mode BEFORE starting TX.
            twr.routingIO2Downloader(TWRClass::TWR_IO_MUX_RADIO);
            radio.transmit();  // Switch radio to TX mode (this should route I2S to RF)
            delay(50);         // Allow a short settling delay.
            sendWavOverRadio();
            radio.receive();   // Revert radio back to RX mode after transmission.
            // Revert I/O routing back to default.
            twr.routingIO2Downloader(TWRClass::TWR_IO_MUX_PIN);
        }
    }
}

// --- Setup Function ---
void setup() {
    bool rlst = false;
    Serial.begin(115200);
    twr.begin();
    
    // Initialize radio based on board revision.
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
    
    // Initialize buttons.
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
    
    // Setup OLED display.
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
    u8g2.print("SENDER");
    u8g2.sendBuffer();
    
    // Set radio frequencies and CTCSS settings.
    radio.setRxFreq(446200000);
    radio.setTxFreq(446200000);
    radio.setRxCXCSS(0);
    radio.setTxCXCSS(0);
    
    // Initialize SD card and BLE.
    initializeSDCard();
    setupBLE();
}

// --- Main Loop ---
void loop() {
    for (uint8_t i = 0; i < COUNT(buttonPins); i++) {
        buttons[i].check();
    }
}
