#include "LilyGo_TWR.h"
#include <U8g2lib.h>

#define RX_FREQUENCY 446200000
#define AUDIO_ALT_PIN A0
#define SAMPLE_DELAY_US 277

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setup() {
    Serial.begin(115200);
    Serial.println("Initialisiere TWR...");

    twr.begin();

    uint8_t addr = twr.getOLEDAddress();
    while (addr == 0xFF) {
        Serial.println("OLED nicht erkannt.");
        delay(1000);
    }
    u8g2.setI2CAddress(addr << 1);
    u8g2.begin();
    u8g2.setFontMode(0);
    u8g2.setFont(u8g2_font_cu12_hr);
    u8g2.setCursor(0, 20);
    u8g2.print("Empfange Audio...");
    u8g2.sendBuffer();

    bool radioInitialized = radio.begin(RadioSerial, twr.getBandDefinition());
    if (radioInitialized) {
        Serial.println("Funkmodul erfolgreich initialisiert.");
        radio.setVolume(8);
        radio.setRxFreq(RX_FREQUENCY);
        radio.setRxCXCSS(0);
        radio.receive();
    } else {
        Serial.println("Fehler beim Initialisieren des Funkmoduls!");
        return;
    }
}

void loop() {
    int rawAudioValue = analogRead(AUDIO_ALT_PIN);
    Serial.println(rawAudioValue);
    delayMicroseconds(SAMPLE_DELAY_US);
}   
