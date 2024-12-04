#include "LilyGo_TWR.h"

// Binary data to send (example)
const uint8_t binaryData[] = {0b10101010, 0b11001100, 0b11110000}; // Sample data
const uint8_t binaryDataLength = sizeof(binaryData);

// Timing and frequencies for FSK modulation
const uint16_t baudRate = 1200; // 
const uint16_t bitDuration = (uint16_t)((1000.0 / baudRate) + 0.5);  // Duration of each bit in ms
const uint16_t freqOne = 2000;        // Frequency for binary "1" 
const uint16_t freqZero = 1000;        // Frequency for binary "0" 

void transmitBinaryDataFSK(uint8_t pin, uint8_t channel, const uint8_t *data, uint8_t length) {
    ledcAttachPin(pin, channel); // Attach the PWM pin to the LEDC channel

    for (uint8_t i = 0; i < length; i++) {
        uint8_t currentByte = data[i];
        
        for (int bit = 7; bit >= 0; bit--) { // Send each bit (MSB first)
            if (currentByte & (1 << bit)) {
                // Logic 1: Generate frequency for "1"
                ledcWriteTone(channel, freqOne);
            } else {
                // Logic 0: Generate frequency for "0"
                ledcWriteTone(channel, freqZero);
            }
            delay(bitDuration); // Wait for the bit duration
            
        }
    }

    ledcDetachPin(pin); // Detach the PWM pin after transmission
}

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

    // Microphone routed to ESP ADC IO15, SA868 audio input routed to ESP IO18
    twr.routingMicrophoneChannel(TWRClass::TWR_MIC_TO_ESP);

    // Start transmission
    radio.transmit();

    transmitBinaryDataFSK(ESP2SA868_MIC, 0, binaryData, binaryDataLength);

    radio.receive();
    

    
}

void loop() {
    // Send binary data using FSK modulation
    //transmitBinaryDataFSK(ESP2SA868_MIC, 0, binaryData, binaryDataLength);
    //delay(3000); // Wait before retransmitting
}
