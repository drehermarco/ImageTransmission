#include "LilyGo_TWR.h"
#include <U8g2lib.h>
#include <SD.h>
#include <AceButton.h>
#include <../Constants.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>


#define TX_FREQUENCY 446200000
#define FREQUENCY_0 800
#define FREQUENCY_1 1800
#define BIT_DURATION 300
#define PAUSE_DURATION 20
#define PTT_PIN 41
#define FILE_NAME "/binary.txt"
#define SEND_INTERVAL 10000

// --- BLE Definitions ---
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_DEVICE_NAME         "T-TWR"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

unsigned long lastSendTime = 0;

// --- AceButton Setup ---
using namespace ace_button;
AceButton                           buttons[3];

const uint8_t buttonPins[] = {
    ENCODER_OK_PIN,    
    BUTTON_PTT_PIN,
    BUTTON_DOWN_PIN
};

void displayText(const char* text) {
    u8g2.clearBuffer();
    u8g2.setCursor(0, 20);
    u8g2.print(text);
    u8g2.sendBuffer();
}

void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState) {
    uint8_t id = button->getId();
    if (id == 1) { // Check if the correct button is pressed
        switch (eventType) {
        case AceButton::kEventPressed:
            radio.transmit(); // Start transmission
            sendBinaryFile();// Send the image binary data
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

void sendBit(char bit) {
    uint16_t frequency = (bit == '0') ? FREQUENCY_0 : FREQUENCY_1;
    
    Serial.print("📡 Sende Bit: ");
    Serial.print(bit);
    Serial.print(" (");
    Serial.print(frequency);
    Serial.println(" Hz)");

    digitalWrite(PTT_PIN, LOW);
    delay(10);

    ledcAttachPin(ESP2SA868_MIC, 0);
    ledcWriteTone(0, frequency);

    delay(BIT_DURATION);

    ledcWriteTone(0, 0);
    ledcDetachPin(ESP2SA868_MIC);

    delay(10);

    digitalWrite(PTT_PIN, HIGH);

    delay(PAUSE_DURATION);
}

void sendBinaryFile() {
    Serial.println("Initialisiere SD-Karte für Lesevorgang...");
    
    if (!SD.begin()) {
        Serial.println("SD-Karten-Fehler!");
        displayText("SD-Fehler!");
        return;
    }

    if (!SD.exists(FILE_NAME)) {
        Serial.println("Datei nicht gefunden!");
        displayText("Keine Datei!");
        return;
    }

    File file = SD.open(FILE_NAME, FILE_READ);
    if (!file) {
        Serial.println("Fehler beim Öffnen von binary.txt!");
        displayText("Fehler: SD");
        return;
    }

    Serial.println("binary.txt geöffnet!");
    displayText("Sende Daten...");

    Serial.println("Sende Startmarker...");
    sendStartMarker();

    while (file.available()) {
        char c = file.read();
        if (c == '0' || c == '1') {
            sendBit(c);
        }
    }

    file.close();
    Serial.println("Übertragung abgeschlossen!");
    displayText("TX Ende.");
}

void sendStartMarker() {
    uint16_t startFrequency = 1200;
    uint16_t startDuration = 500;
    
    Serial.print("Sende Startmarker: ");
    Serial.print(startFrequency);
    Serial.println(" Hz");

    digitalWrite(PTT_PIN, LOW);

    ledcAttachPin(ESP2SA868_MIC, 0);
    ledcWriteTone(0, startFrequency);

    delay(startDuration);

    ledcWriteTone(0, 0);
    ledcDetachPin(ESP2SA868_MIC);

    digitalWrite(PTT_PIN, HIGH);

    delay(50);
}

// --- Global File Handle for BLE Reception ---
File textFile;

// --- BLE Server Callbacks ---
class MyBLEServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    Serial.println("BLE client connected, opening text file for writing.");
    textFile = SD.open(FILE_NAME, FILE_WRITE);
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

void setup() {
    Serial.begin(115200);
    Serial.println("Initialisiere TWR...");

    twr.begin();

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
        Serial.println("OLED nicht erkannt.");
        delay(1000);
    }
    u8g2.setI2CAddress(addr << 1);
    u8g2.begin();
    u8g2.setFontMode(0);
    u8g2.setFont(u8g2_font_cu12_hr);
    displayText("FSK Sender");

    if (!SD.begin()) {
        Serial.println("SD-Karten-Fehler!");
        displayText("SD-Fehler!");
        return;
    }
    Serial.println("SD-Karte OK.");

    bool radioInitialized;// = radio.begin(RadioSerial, twr.getBandDefinition());

    if (twr.getVersion() == TWRClass::TWR_REV2V1) {
        Serial.println("Detection using TWR Rev2.1");
        radio.setPins(SA868_PTT_PIN, SA868_PD_PIN);
        radioInitialized = radio.begin(RadioSerial, twr.getBandDefinition());
    } 

    if (radioInitialized) {
        Serial.println("Funkmodul bereit.");
        radio.setVolume(1);
        radio.setTxFreq(TX_FREQUENCY);
        radio.setRxFreq(TX_FREQUENCY);
        radio.setRxCXCSS(0);
        radio.setTxCXCSS(0);
    } else {
        Serial.println("Fehler beim Initialisieren des Funkmoduls!");
        displayText("Funk-Fehler!");
        return;
    }

    pinMode(PTT_PIN, OUTPUT);
    digitalWrite(PTT_PIN, HIGH);

    twr.routingMicrophoneChannel(TWRClass::TWR_MIC_TO_ESP);

    displayText("Warte auf Sendung...");
    setupBLE();
}

void loop() {
    /*
    if (millis() - lastSendTime >= SEND_INTERVAL) {
        Serial.println("Starte neue Sendung...");
        sendBinaryFile();
        lastSendTime = millis();
    }
        */
    for (uint8_t i = 0; i < COUNT(buttonPins); i++) {
        buttons[i].check();
    }
}
