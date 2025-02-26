#include "LilyGo_TWR.h"
#include <U8g2lib.h>
#include <SD.h>
#include <AceButton.h>
#include <../Constants.h>
#include <Arduino.h>

#define TX_FREQUENCY 446200000
#define FREQUENCY_0 800
#define FREQUENCY_1 1800
#define BIT_DURATION 300
#define PAUSE_DURATION 20
#define PTT_PIN 41
#define FILE_NAME "/binary.txt"
#define SEND_INTERVAL 10000

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
