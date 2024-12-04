#include <U8g2lib.h>
#include <Rotary.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <BLEServer.h>
#include <WiFiAP.h>
#include <SD.h>
#include "BLE.h"
#include <AceButton.h>
#include <Adafruit_Sensor.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include "Constants.h"


#define DEBUG_PORT Serial    // Output debugging information
#include "LilyGo_TWR.h"

using namespace ace_button;

enum Button {
    ShortPress,
    LongPress,
    Unknown
};

#define DECLARE_DEMO(function)      void function(uint8_t menuSelect)
#define U8G2_HOR_ALIGN_CENTER(t)    ((u8g2.getDisplayWidth() - (u8g2.getUTF8Width(t))) / 2)
#define U8G2_HOR_ALIGN_RIGHT(t)     (u8g2.getDisplayWidth()  -  u8g2.getUTF8Width(t))


DECLARE_DEMO(demoSpeaker);
DECLARE_DEMO(demoOled);
DECLARE_DEMO(demoSDCard);
DECLARE_DEMO(demoTransFreq);
DECLARE_DEMO(demoRecvFreq);
DECLARE_DEMO(demoBLE);
DECLARE_DEMO(demoSetting);
DECLARE_DEMO(drawError);


uint32_t readRotary(uint32_t cur, uint32_t minOut, uint32_t maxOut, uint32_t steps = 1 );
void printMain();
void printPowerOFF();
Button readButton();

struct demo_struct {
    const char      *demo_item;
    const char      *demo_description;
    const uint8_t   demo_icon[32];
    void            (*demo_func)(uint8_t menuSelect);
} demo [] = {
    //* {
    //*         Title,
    //*         Description,
    //*         XBM ICON,
    //*         Callback,
    //* },
    {
        "SPEAKER",
        "PLAYING GAME MELODIES",
        { 0x00, 0x00, 0x80, 0x01, 0xc0, 0x01, 0xe0, 0x01, 0xb0, 0x11, 0x9e, 0x21, 0x8e, 0x45, 0x86, 0x49, 0x86, 0x49, 0x8e, 0x45, 0x9e, 0x21, 0xb0, 0x11, 0xe0, 0x01, 0xc0, 0x01, 0x80, 0x01, 0x00, 0x00 },
        demoSpeaker
    },
    {
        "STORAGE",
        "SD CARD INFORMATION",
        {0x00, 0x00, 0xc0, 0x1f, 0xe0, 0x3f, 0x70, 0x35, 0x78, 0x35, 0xfc, 0x3f, 0xfc, 0x3f, 0xfc, 0x3f, 0xfc, 0x3f, 0xfc, 0x3f, 0xfc, 0x3f, 0xfc, 0x3f, 0xfc, 0x3f, 0xfc, 0x3f, 0xf8, 0x1f, 0x00, 0x00},
        demoSDCard
    },
    {
        "TRANS FREQ",
        "RADIO TRANS FREQ",
        {0x00, 0x00, 0x00, 0x0c, 0x00, 0x1c, 0x00, 0x3c, 0xc0, 0x7f, 0x30, 0xe0, 0x38, 0xe0, 0xfc, 0x77, 0xee, 0x3f, 0x06, 0x1c, 0x06, 0x0c, 0xfc, 0x03, 0xf8, 0x01, 0x30, 0x00, 0x20, 0x00, 0x00, 0x00},
        demoTransFreq
    },

    {
        "RECV FREQ",
        "RADIO RECV FREQ",
        {0x00, 0x00, 0x00, 0x0c, 0x00, 0x1c, 0x00, 0x3c, 0xc0, 0x7f, 0x30, 0xe0, 0x38, 0xe0, 0xfc, 0x77, 0xee, 0x3f, 0x06, 0x1c, 0x06, 0x0c, 0xfc, 0x03, 0xf8, 0x01, 0x30, 0x00, 0x20, 0x00, 0x00, 0x00},
        demoRecvFreq
    },
    {
        "BLUETOOTH",
        "BLUETOOTH CONFIGURE",
        {0x80, 0x01, 0x80, 0x03, 0x80, 0x07, 0x98, 0x0f, 0xb8, 0x1d, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0xc0, 0x03, 0xe0, 0x07, 0xf0, 0x0f, 0xb8, 0x1d, 0x98, 0x0f, 0x80, 0x07, 0x80, 0x03, 0x80, 0x01},
        demoBLE
    },
    {
        "SETTING",
        "SYSTEM SETTING",
        {0x00, 0x00, 0xc0, 0x03, 0xc0, 0x03, 0xfc, 0x3f, 0x7c, 0x3e, 0xc6, 0x63, 0xee, 0x77, 0x6c, 0x36, 0x6c, 0x36, 0xee, 0x77, 0xc6, 0x63, 0x7c, 0x3e, 0xfc, 0x3f, 0xc0, 0x03, 0xc0, 0x03, 0x00, 0x00},
        demoSetting
    },
};

const uint8_t itemsMENU = COUNT(demo);

struct RotarySetting {
    uint32_t cur;
    uint32_t max;
    uint32_t min;
    uint32_t steps;
};

U8G2_SH1106_128X64_NONAME_F_HW_I2C  u8g2(U8G2_R0, U8X8_PIN_NONE);
Rotary                              rotary = Rotary(ENCODER_A_PIN, ENCODER_B_PIN);
TinyGPSPlus                         gps;
AceButton                           buttons[3];
Button                              state = Unknown;
QueueHandle_t                       rotaryMsg;
QueueHandle_t                       rotarySetting;
TaskHandle_t                        rotaryHandler;
bool                                inMenu = true;

const char                          *ssid = "T-TWR";
const char                          *password = "12345678";

const uint8_t                       buttonPins [] = {
    ENCODER_OK_PIN,
    BUTTON_PTT_PIN,
    BUTTON_DOWN_PIN
};

// void endWeb()

// void setupWeb()

void setRotaryValue(uint32_t cur, uint32_t min, uint32_t max, uint32_t steps)
{
    static RotarySetting rSetting = {0};
    rSetting.cur = cur;
    rSetting.max = max;
    rSetting.min = min;
    rSetting.steps = steps;
    xQueueOverwrite(rotarySetting, (void *)&rSetting);
}


void rotaryTask(void *p)
{
    RotarySetting setting = {0};
    uint32_t pos = 0;

    rotary.begin();

    while (1) {

        uint8_t result = rotary.process();

        if (xQueueReceive(rotarySetting, (void *)&setting, ( TickType_t ) 2) == pdPASS) {
            pos = setting.cur;
            xQueueOverwrite(rotaryMsg, (void *)&pos);
            continue;
        }

        if (result) {
            if (result == DIR_CW) {
                pos += setting.steps;
                DBG("Up");
            } else {
                if (pos != 0) {
                    pos -= setting.steps;
                }
                DBG("Down");
            }
            if (pos <= setting.min) {
                pos = setting.min;
            }
            if (pos >= setting.max) {
                pos = setting.max;
            }
            xQueueOverwrite(rotaryMsg, (void *)&pos);
        }
        delay(2);
    }
}

void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
    uint8_t id = button->getId();

    DBG(F("EventType: "), AceButton::eventName(eventType), F("; buttonState: "), buttonState, F("; ID: "), id);

    // rotary center button
    switch (id) {
    case 0: {
        switch (eventType) {
        case AceButton::kEventClicked:
            state = ShortPress;
            DBG("ShortPress");
            break;
        case AceButton::kEventLongPressed:
            state = LongPress;
            DBG("LongPress");
            break;
        default:
            break;
        }
    }
    break;

    // PTT button
    case 1:
        switch (eventType) {
        case AceButton::kEventPressed:
            DBG("transmit");
            radio.transmit();
            break;
        case AceButton::kEventReleased:
            DBG("receive");
            radio.receive();
            break;
        default:
            break;
        }
        break;

    // BOOT button
    case 2:
        switch (eventType) {
        case AceButton::kEventPressed:
            if (!inMenu) {
                state = LongPress;
                DBG("LongPress");
            } else {
                uint8_t v = radio.getVolume();
                radio.setVolume(--v);
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

bool setupSDCard()
{
    if (SD.begin(SD_CS, SPI)) {
        uint8_t cardType = SD.cardType();
        if (cardType != CARD_NONE) {
            return true;
        }
    }
    return false;
}

// bool setupBME280()

void setupOLED(uint8_t addr)
{
    u8g2.setI2CAddress(addr << 1);
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setContrast(10);
    uint8_t b = 10;
    u8g2.firstPage();
    do {
        do {
            u8g2.setFont(u8g2_font_tenstamps_mu);
            u8g2.drawXBM(40, 0, 48, 48, xbmLOGO);
            u8g2.setCursor(22, 62);
            u8g2.print("LILYGO");
            u8g2.sendBuffer();
            u8g2.setContrast(b);
        } while (u8g2.nextPage());
        b += 10;
        delay(8);
    } while (b < twr.pdat.dispBrightness);
}

void setup()
{
    bool rslt = false;

#ifdef DEBUG_PORT
    DEBUG_PORT.begin(115200);
#endif

    // Create message queue
    rotaryMsg = xQueueCreate(1, sizeof(uint32_t));
    rotarySetting = xQueueCreate(1, sizeof(RotarySetting));

  
    //* Initializing PMU is related to other peripherals
    //* Automatically detect the revision version through GPIO detection.
    //* If GPIO2 has been externally connected to other devices, the automatic detection may not work properly.
    //* Please initialize with the version specified below.
    //* Rev2.1 is not affected by GPIO2 because GPIO2 is not exposed in Rev2.1
    rslt = twr.begin();
    
    

    //* If GPIO2 is already connected to other devices, please initialize it with the specified version.
    //rslt =  twr.begin(LILYGO_TWR_REV2_0);

    while (!rslt) {
        DBG("PMU communication failed..");
        delay(1000);
    }


    //* Rev2.1 uses OLED to determine whether it is VHF or UHF.
    //* Rev2.0 cannot determine by device address.By default, the 0X3C device address is used.

    if (twr.getVersion() == TWRClass::TWR_REV2V1) {
        DBG("Detection using TWR Rev2.1");

        //* Initialize SA868
        radio.setPins(SA868_PTT_PIN, SA868_PD_PIN);
        rslt = radio.begin(RadioSerial, twr.getBandDefinition());

    } else {

        DBG("Detection using TWR Rev2.0");

        //* Rev2.0 cannot determine whether it is UHF or VHF, and can only specify the initialization BAND.
        //* Or modify it through the menu INFO -> BAND
        radio.setPins(SA868_PTT_PIN, SA868_PD_PIN, SA868_RF_PIN);

        //* Designated as UHF
        rslt = radio.begin(RadioSerial, SA8X8_UHF);

        //* Designated as VHF
        // rslt = radio.begin(RadioSerial, SA8X8_VHF);

        // If BAND is not specified, the default BAND is used, set through the menu.
        // rslt = radio.begin(RadioSerial, SA8X8_UNKNOW);
    }

    // If the display does not exist, it will block here
    uint8_t addr = twr.getOLEDAddress();
    if ((addr != 0x3C || addr != 0x3D) && !rslt) {
        // Initialize display
        u8g2.setI2CAddress(addr << 1);
        u8g2.begin();
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_tenstamps_mu);
        u8g2.setCursor(30, 28);
        u8g2.print("SA8X8");
        u8g2.setCursor(22, 52);
        u8g2.print("FAILED");
        u8g2.sendBuffer();
    }
    while (!rslt) {
        DBG("SA8x8 communication failed, please use ATDebug to check whether the module responds normally..");
        delay(300);
    }
  
    // Initialize SD card
    if (setupSDCard()) {
        uint8_t cardType = SD.cardType();
        DBG("SD_MMC Card Type: ");
        if (cardType == CARD_MMC) {
            DBG("MMC");
        } else if (cardType == CARD_SD) {
            DBG("SDSC");
        } else if (cardType == CARD_SDHC) {
            DBG("SDHC");
        } else {
            DBG("UNKNOWN");
        }
#ifdef DEBUG_PORT
        uint32_t cardSize = SD.cardSize() / (1024 * 1024);
        uint32_t cardTotal = SD.totalBytes() / (1024 * 1024);
        uint32_t cardUsed = SD.usedBytes() / (1024 * 1024);
        DBG("SD Card Size:", cardSize, "MB");
        DBG("Total space:",  cardTotal, "MB");
        DBG("Used space:",   cardUsed, "MB");
#endif
    }

    // Initialize display
    setupOLED(addr);

    delay(3000);

    // Initialize button
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

    // Disable the PMU long press shutdown function.
    twr.enablePowerOff(false);

    // Added display of shutdown animation after long pressing the PWR button
    twr.attachLongPressed([]() {
        printPowerOFF();
    });

    // Add a callback function after pressing the PWR button
    twr.attachClick([]() {
        if (inMenu) {
            uint8_t v = radio.getVolume();
            radio.setVolume(++v);
        }
    });

    // Create a rotary encoder processing task
    xTaskCreate(rotaryTask, "rotary", 10 * 1024, NULL, 10, &rotaryHandler);

}


void loop()
{
    static uint8_t menuSelect     = 0;
    static uint8_t prevMenuSelect = menuSelect;
    Button btnPressed;

    inMenu = true;
    do {
        btnPressed = readButton();
        printMain();

        // DeepSleep test , About ~660uA
        if (btnPressed == LongPress) {
            DBG("DeepSleep....");
            printDeepSleep();

            radio.sleep();

            twr.sleep();

            esp_sleep_enable_ext1_wakeup(_BV(BUTTON_PTT_PIN), ESP_EXT1_WAKEUP_ALL_LOW);

            esp_deep_sleep_start();
        }

    } while ( btnPressed != ShortPress );

    inMenu = false;
    uint8_t lastItem = 0;

    if (twr.isEnableBeep()) {
        twr.routingSpeakerChannel(TWRClass::TWR_ESP_TO_SPK);
    }

    while (1) {
        do {
            menuSelect  = readRotary(lastItem, 0, itemsMENU - 1);
            btnPressed = readButton();
            if (btnPressed == LongPress) {
                menuSelect = 0;
                break;
            }
            if ( menuSelect != prevMenuSelect ) {
                prevMenuSelect = menuSelect;
                beep();
            }
            printMenu(menuSelect);

        } while ( btnPressed != ShortPress );

        beep();

        if (btnPressed == ShortPress) {
            demo[menuSelect].demo_func(menuSelect);
            lastItem = menuSelect;
        } else {
            break;
        }
    }
    menuSelect = 0;
    if (twr.isEnableBeep()) {
        twr.routingSpeakerChannel(TWRClass::TWR_RADIO_TO_SPK);
    }
}

void printPowerOFF()
{
    int b = twr.pdat.dispBrightness;
    do {
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_tenstamps_mu);
            u8g2.drawXBM(40, 0, 48, 48, xbmOFF);
            u8g2.setCursor(5, 59);
            u8g2.print("POWEROFF");
            u8g2.setContrast(b);
        } while ( u8g2.nextPage() );
        b -= 10;
        delay(8);
    } while (b >= 0);
    twr.shutdown();
}

void printDeepSleep()
{
    int b = twr.pdat.dispBrightness;
    do {
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_tenstamps_mu);
            u8g2.drawXBM(40, 0, 48, 48, xbmOFF);
            u8g2.setCursor(32, 59);
            u8g2.print("SLEEP");
            u8g2.setContrast(b);
        } while ( u8g2.nextPage() );
        b -= 10;
        delay(8);
    } while (b >= 0);

    u8g2.setPowerSave(true);
}

void printMain()
{
    uint8_t rssi_x = 95;
    uint8_t rssi_y = 12;
    uint8_t bat_x = 110;
    uint8_t bat_y = 12;
    static int rssi = 0;
    static uint32_t check_interval = 0;
    int percentage = 0;

    u8g2.firstPage();
    do {
        drawFrame();

        bool isTransmit = radio.isTransmit();
        // Draw RSSI
        if (!isTransmit) {
            if (millis() > check_interval) {
                rssi = radio.getRSSI();
                check_interval = millis() + 1000;
            }
        }

        percentage = twr.getBatteryPercent();

        u8g2.setFont(u8g2_font_siji_t_6x10);

        if (rssi >= 80) {
            u8g2.drawGlyph(rssi_x, rssi_y, 0xe261);
        } else if (rssi >= 40) {
            u8g2.drawGlyph(rssi_x, rssi_y, 0xe260);
        } else if (rssi >= 20) {
            u8g2.drawGlyph(rssi_x, rssi_y, 0xe25f);
        } else if (rssi >= 10) {
            u8g2.drawGlyph(rssi_x, rssi_y, 0xe25e);
        } else {
            u8g2.drawGlyph(rssi_x, rssi_y, 0xe25d);
        }

        // Battery
        if (twr.isCharging()) {
            u8g2.drawGlyph(bat_x, bat_y, 0xe239);
        } else if (percentage < 0) {
            u8g2.drawGlyph(bat_x, bat_y, 0xe242);
        } else if (percentage >= 80) {
            u8g2.drawGlyph(bat_x, bat_y, 0xe251);
        } else if (percentage >= 40) {
            u8g2.drawGlyph(bat_x, bat_y, 0xe250);
        } else if (percentage >= 20) {
            u8g2.drawGlyph(bat_x, bat_y, 0xe24f);
        } else if (percentage >= 10) {
            u8g2.drawGlyph(bat_x, bat_y, 0xe24e);
        } else {
            u8g2.drawGlyph(bat_x, bat_y, 0xe24d);
        }

        // GPS
        u8g2.drawGlyph(80, 12, 0xe02c);

        int start_pos = 70;
        // Ble
        if (twr.getSetting().ble) {
            u8g2.drawGlyph(start_pos, 12, 0xe00b);
            start_pos -= 13;
        }
        // Wifi
        if (twr.getSetting().wifi) {
            u8g2.drawGlyph(start_pos, 12, 0xe21a);
            start_pos -= 10;
        }
        // SD
        if (SD.cardType() != CARD_NONE
                && SD.cardType() != CARD_UNKNOWN) {
            u8g2.drawGlyph(start_pos - 2, 11, 0xE120);
            start_pos -= 10;
        }

        if (isTransmit) {
            u8g2.drawGlyph(start_pos, 12, 0xe04C);
        }


        int volume = radio.getVolume();
        for (int i = 0; i < volume; ++i) {
            u8g2.drawGlyph(5 + (4 * i), 12, 0x007c);
        }

        float transFreq = radio.getStetting().transFreq / 1000000.0;
        float recvFreq  = radio.getStetting().recvFreq / 1000000.0;

        // Draw Freq
        u8g2.setFont(u8g2_font_pxplusibmvga8_mr    );
        u8g2.setCursor(22, 35);
        u8g2.print("TX:");
        u8g2.setCursor(22, 50);
        u8g2.print("RX:");

        u8g2.setFont(u8g2_font_9x6LED_mn);
        u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(transFreq, 4).c_str()) - 21, 35 );
        u8g2.print(transFreq, 4);
        u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(recvFreq, 4).c_str()) - 21, 50 );
        u8g2.print(recvFreq, 4);

    } while ( u8g2.nextPage() );
}

void printMenu( uint8_t menuSelect )
{
    u8g2.firstPage();
    do {
        //ROW 1:
        if ( menuSelect > 0 ) {
            u8g2.drawXBMP(4, 2,  14, 14, xMenuUp);
            u8g2.setFont(u8g2_font_haxrcorp4089_tr);
            u8g2.setCursor(22, 13);
            u8g2.print(demo[menuSelect - 1].demo_item);

        }
        //ROW 2:
        u8g2.setFont(u8g2_font_nokiafc22_tu);
        u8g2.drawBox(0, 18, 128, 27);

        u8g2.setColorIndex(0);
        u8g2.drawXBMP(3, 24, 16, 16, demo[menuSelect].demo_icon);

        if ( strlen(demo[menuSelect].demo_description) == 0 ) {
            u8g2.drawStr(22, 35, demo[menuSelect].demo_item);
        } else {
            u8g2.drawStr(22, 30, demo[menuSelect].demo_item);
            u8g2.setFont(u8g2_font_micro_mr);
            u8g2.drawStr(22, 40, demo[menuSelect].demo_description);
        }

        //ROW 3:
        u8g2.setColorIndex(1);
        if ( menuSelect < itemsMENU - 1 ) {
            u8g2.drawXBMP(4, 48, 14, 14, xMenuDown);
            u8g2.setFont(u8g2_font_haxrcorp4089_tr);
            u8g2.setCursor(22, 58);
            u8g2.print(demo[menuSelect + 1].demo_item);
        }
        drawFrame();
    } while ( u8g2.nextPage() );
}

// void demoLed( uint8_t menuSelect )

void demoSpeaker( uint8_t menuSelect )
{
    Button   btnPressed;
    uint32_t prevTime   = 0;
    int value = 600;

    if (!twr.isEnableBeep()) {
        twr.routingSpeakerChannel(TWRClass::TWR_ESP_TO_SPK);
    }

    do {
        value   = readRotary(value, 100, 3000, 100);
        btnPressed = readButton();

        u8g2.firstPage();
        do {
            drawFrame();
            drawHeader(menuSelect);

            u8g2.drawFrame(13, 30, 102, 12);
            u8g2.drawFrame(14, 31, 100, 10);

            uint32_t bar = map(value, 100, 3000, 0, 100);
            for ( int x = 0 ; x < bar ; x++ ) {
                u8g2.drawVLine(15 + x, 32, 8);
            }

            u8g2.setFont(u8g2_font_nokiafc22_tu);
            u8g2.setCursor(14, 51);
            u8g2.print(F("FREQ:"));

            u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(bar).c_str()) - 35, 51 );
            u8g2.print(value); u8g2.print("HZ");

        } while ( u8g2.nextPage() );

        if (millis() > prevTime) {
            tone(ESP32_PWM_TONE, value, 100);
            prevTime = millis() + 300;
        }

    } while (btnPressed != LongPress);

    if (!twr.isEnableBeep()) {
        twr.routingSpeakerChannel(TWRClass::TWR_RADIO_TO_SPK);
    }
}

void demoOled( uint8_t menuSelect )
{
    int prevValue = 0;
    int value = twr.pdat.dispBrightness;
    Button btnPressed;
    do {

        value   = readRotary(value, 0, 250, 10);
        btnPressed = readButton();
        if (prevValue != value) {
            prevValue  = value;
            beep();
            u8g2.setContrast(value);
        }

        u8g2.firstPage();
        do {
            drawFrame();
            drawHeader(menuSelect);
            u8g2.drawFrame(13, 30, 102, 12);
            u8g2.drawFrame(14, 31, 100, 10);
            uint32_t bar = map(value, 0, 250, 0, 100);
            for ( int x = 0 ; x < bar ; x++ ) {
                u8g2.drawVLine(15 + x, 32, 8);
            }
            u8g2.setFont(u8g2_font_nokiafc22_tu);
            u8g2.setCursor(14, 51);
            u8g2.print(F("BRIGHTNESS:"));
            u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(bar).c_str()) - 14, 51 );
            u8g2.print(value);
        } while (u8g2.nextPage());
    } while ( btnPressed != LongPress );

    SAVE_CONFIGURE(dispBrightness, value);
}

// void demoButton( uint8_t menuSelect )

// void demoAlarm(uint8_t menuSelect )

// void demoSensor( uint8_t menuSelect )

// void demoGPS(uint8_t menuSelect)

void demoSDCard(uint8_t menuSelect)
{
    Button   btnPressed;
    uint32_t cardSize = 0;
    uint32_t cardTotal = 0;
    uint32_t cardUsed = 0;
    uint32_t intervalue = 0;

    do {
        btnPressed = readButton();
        u8g2.firstPage();
        do {

            if (SD.cardType() == CARD_NONE) {
                if (millis() > intervalue) {
                    if (!setupSDCard()) {
                        cardSize = 0;
                        cardTotal = 0;
                        cardUsed = 0;
                    }
                    intervalue = millis() + 8000UL;
                }
            }
            if (SD.cardType() != CARD_NONE && cardSize == 0) {
                cardSize  = SD.cardSize() / (1024 * 1024);
                cardTotal = SD.totalBytes() / (1024 * 1024);
                cardUsed  = SD.usedBytes() / (1024 * 1024);
            }

            drawFrame();
            drawHeader(menuSelect);

            u8g2.setFont(u8g2_font_nokiafc22_tu);

            u8g2.setCursor(14, 31);
            u8g2.print(F("SIZE:"));
            u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(cardSize).c_str()) - 22, 31 );
            u8g2.print(cardSize);
            u8g2.setCursor(109, 31);
            u8g2.print(F("MB"));

            u8g2.setCursor(14, 44);
            u8g2.print(F("TOTAL:"));
            u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(cardTotal).c_str()) - 22, 44 );
            u8g2.print(cardTotal);
            u8g2.setCursor(109, 44);
            u8g2.print(F("MB"));

            u8g2.setCursor(14, 57);
            u8g2.print(F("USED:"));
            u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(cardUsed).c_str()) - 22, 57 );
            u8g2.print(cardUsed);
            u8g2.setCursor(109, 57);
            u8g2.print(F("MB"));
        } while ( u8g2.nextPage() );

    } while ( btnPressed != LongPress );
}

// void demoPMU(uint8_t menuSelect)

uint32_t demoFreqSetting(uint8_t menuSelect,
                         bool tx,
                         uint32_t curFreq,
                         uint8_t option,
                         uint32_t minOut,
                         uint32_t maxOut,
                         uint32_t steps
                        )
{
    Button btnPressed;
    uint32_t value = 0;
    uint32_t prevValue = 0;
    uint32_t bandwidth = radio.getStetting().bandwidth ;
    uint32_t  transFreq = tx ? radio.getStetting().transFreq : radio.getStetting().recvFreq;
    uint32_t CXCSS = tx ? radio.getStetting().txCXCSS : radio.getStetting().rxCXCSS;
    do {
        btnPressed = readButton();
        value = readRotary(curFreq, minOut, maxOut, steps);
        if ( btnPressed == ShortPress ) {
            beep();
        }
        if (prevValue != value) {
            beep();
            prevValue = value;
        }
        u8g2.firstPage();
        do {

            drawFrame();
            drawHeader(menuSelect);

            u8g2.setFont(u8g2_font_nokiafc22_tu);

            u8g2.setCursor(14, 31);
            u8g2.print(F("FREQ  :"));

            if (option == 0) {
                u8g2.drawRBox(55, 22, 58, 12, 2);
                u8g2.setColorIndex(0);
                u8g2.setCursor(U8G2_HOR_ALIGN_RIGHT(String(value / 1000000.0, 5).c_str()) - 22, 31 );
                u8g2.print(value / 1000000.0, 5);
                u8g2.setColorIndex(1);
            } else {
                u8g2.setCursor(U8G2_HOR_ALIGN_RIGHT(String(transFreq / 1000000.0, 5).c_str()) - 22, 31 );
                u8g2.print(transFreq / 1000000.0, 5);
            }

            u8g2.setCursor(14, 44);
            u8g2.print(F("STEPS:"));
            if (option == 1) {
                u8g2.drawRBox(55, 35, 58, 12, 2);
                u8g2.setColorIndex(0);
                u8g2.setCursor(U8G2_HOR_ALIGN_RIGHT(String(value / 1000.0, 1).c_str()) - 45, 44 );
                u8g2.print(value / 1000.0, 1);
                u8g2.print("KHZ");
                u8g2.setColorIndex(1);
            } else {
                u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(bandwidth / 1000.0, 1).c_str()) - 45, 44 );
                u8g2.print(bandwidth / 1000.0, 1);
                u8g2.print("KHZ");
            }

            u8g2.setCursor(14, 57);
            u8g2.print(F("CXCSS:"));
            if (option == 2) {
                u8g2.drawRBox(55, 48, 58, 12, 2);
                u8g2.setColorIndex(0);
                u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(cxcss[value]).c_str()) - 38, 57 );
                u8g2.print(cxcss[value]);
                u8g2.setColorIndex(1);
            } else {
                u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(cxcss[CXCSS]).c_str()) - 38, 57 );
                u8g2.print(cxcss[CXCSS]);
            }
        } while (u8g2.nextPage());

    } while ( btnPressed != ShortPress );
    if (value < 446000000)
        value = 446000000;
    else if (value > 446200000)
        value = 446200000;
    return  value ;
}

void setTransFreq(uint8_t menuSelect, bool tx)
{
    Button btnPressed;
    uint32_t pos = 0;
    uint32_t prevPos = 0;
    uint32_t minOut = radio.getStetting().minFreq;
    uint32_t maxOut = radio.getStetting().maxFreq;
    uint32_t transFreq = tx ? radio.getStetting().transFreq : radio.getStetting().recvFreq;
    uint32_t bandwidth = radio.getStetting().bandwidth ;
    uint32_t CXCSS = tx ? radio.getStetting().txCXCSS : radio.getStetting().rxCXCSS;

    do {
        btnPressed = readButton();
        pos = readRotary(0, 0, 2);

        if (prevPos != pos) {
            prevPos  = pos;
            beep();
        }

        u8g2.firstPage();
        do {
            drawFrame();
            drawHeader(menuSelect);
            u8g2.setFont(u8g2_font_nokiafc22_tu);

            switch (pos) {
            case 0:
                u8g2.drawRFrame(55, 22, 58, 12, 2);
                break;
            case 1:
                u8g2.drawRFrame(55, 35, 58, 12, 2);
                break;
            case 2:
                u8g2.drawRFrame(55, 48, 58, 12, 2);
                break;
            default:
                break;
            }
            u8g2.setCursor(14, 31);
            u8g2.print(F("FREQ  :"));
            u8g2.setCursor(U8G2_HOR_ALIGN_RIGHT(String(transFreq / 1000000.0, 5).c_str()) - 22, 31 );
            u8g2.print(transFreq / 1000000.0, 5);

            u8g2.setCursor(14, 44);
            u8g2.print(F("STEPS:"));
            u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(bandwidth / 1000.0, 1).c_str()) - 45, 44 );
            u8g2.print(bandwidth / 1000.0, 1);
            u8g2.print("KHZ");

            u8g2.setCursor(14, 57);
            u8g2.print(F("CXCSS:"));
            u8g2.setCursor( U8G2_HOR_ALIGN_RIGHT(String(cxcss[CXCSS]).c_str()) - 38, 57 );
            u8g2.print(cxcss[CXCSS]);

        } while ( u8g2.nextPage() );

        if (btnPressed == ShortPress ) {

            beep();

            uint32_t steps = bandwidth;
            uint32_t cur = transFreq;

            switch (pos) {
            case 0:
                minOut = radio.getStetting().minFreq;
                maxOut = radio.getStetting().maxFreq;
                steps = radio.getStetting().bandwidth ;
                transFreq = tx ? radio.getStetting().transFreq : radio.getStetting().recvFreq;
                break;
            case 1:
                minOut = 12500;
                maxOut = 25000;
                steps =  12500;
                bandwidth = radio.getStetting().bandwidth ;
                cur = bandwidth;
                break;
            case 2:
                minOut = 0;
                maxOut = COUNT(cxcss);
                steps =  1;
                cur = CXCSS;
                break;
            default:
                break;
            }

            uint32_t setValue = demoFreqSetting(menuSelect, tx, cur, pos, minOut, maxOut, steps);
            DBG("Set opt:", pos, "value:", setValue);

            switch (pos) {
            case 0:
                tx ? radio.setTxFreq(setValue) : radio.setRxFreq(setValue);
                break;
            case 1:
                if (radio.getStetting().bandwidth != setValue) {
                    radio.setBandWidth(setValue);
                    // Bandwidth change, reset frequency to the minimum frequency value
                    tx ? radio.setTxFreq(radio.getStetting().minFreq) : radio.setRxFreq(radio.getStetting().minFreq);
                }
                break;
            case 2:
                tx ? radio.setTxCXCSS(setValue) : radio.setRxCXCSS(setValue);
                break;
            default:
                break;
            }
            transFreq = tx ? radio.getStetting().transFreq : radio.getStetting().recvFreq;
            bandwidth = radio.getStetting().bandwidth ;
            CXCSS = tx ? radio.getStetting().txCXCSS : radio.getStetting().rxCXCSS;
        }

    } while ( btnPressed != LongPress );

    radio.saveConfigure();
}

void demoTransFreq(uint8_t menuSelect)
{
    setTransFreq(menuSelect, true);
}

void demoRecvFreq(uint8_t menuSelect)
{
    setTransFreq(menuSelect, false);
}

// void demoSquelchLevel(uint8_t menuSelect)

// void demoFilter(uint8_t menuSelect)

void demoBLE(uint8_t menuSelect)
{
    Button btnPressed;
    int prevValue = 0;
    int value = BLE::isRunning();
    do {
        value           = readRotary(value, 0, 1);
        btnPressed      = readButton();
        u8g2.firstPage();
        do {
            drawFrame();
            drawHeader(menuSelect);
            if (prevValue != value) {
                prevValue = value;
                beep();
            }
            u8g2.setFont(u8g2_font_nokiafc22_tu);

            u8g2.setCursor(14, 31);
            u8g2.print("NAME:");
            if (BLE::isRunning()) {
                u8g2.print(BLE::getDevName());
            } else {
                u8g2.print("N/A:");
            }

            u8g2.setCursor(14, 44);
            u8g2.print(F("[   ]  OFF"));
            u8g2.setCursor(14, 57);
            u8g2.print(F("[   ]  ON"));
            u8g2.setCursor(19, 44);
            u8g2.print(value == 0 ? F("*") : F(" "));
            u8g2.setCursor(19, 57);
            u8g2.print(value == 1 ? F("*") : F(" "));

        } while ( u8g2.nextPage() );

        if (btnPressed == ShortPress) {
            beep();
            if (value) {
                BLE::enableBLE();
            } else {
                BLE::disableBLE();
            }
            value = BLE::isRunning();
        }
    } while ( btnPressed != LongPress );
    SAVE_CONFIGURE(ble, value);
}

// void demoWiFi(uint8_t menuSelect)

void demoSetting(uint8_t menuSelect)
{
    Button btnPressed;
    int value = 0;
    int prevValue = 0;
    bool  beep = twr.getSetting().beep;

    if (twr.getVersion() == TWRClass::TWR_REV2V0) {
        drawError(menuSelect); return;
    }

    do {
        value       = readRotary(0, 0, 2);
        btnPressed  = readButton();

        u8g2.firstPage();
        do {
            drawFrame();
            drawHeader(menuSelect);

            if (prevValue != value) {
                prevValue = value;
            }
            u8g2.setFont(u8g2_font_nokiafc22_tu);

            u8g2.setCursor(14, 31);
            if (beep) {
                u8g2.print(F("   *    ")) ;
            } else {
                u8g2.print(F("   -    ")) ;
            }
            u8g2.setCursor(45, 31);
            u8g2.print(F("BEEP")) ;
            switch (value) {
            case 0:
                u8g2.setCursor(14, 31);
                if (beep) {
                    u8g2.print(F("[  *  ]")) ;
                } else {
                    u8g2.print(F("[  -  ]")) ;
                }
                break;
            default:
                break;
            }

            if (btnPressed == ShortPress) {
                switch (value) {
                case 0:
                    twr.enableBeep(!beep);
                    beep = twr.getSetting().beep;
                    if (twr.isEnableBeep()) {
                        twr.routingSpeakerChannel(TWRClass::TWR_ESP_TO_SPK);
                    } else {
                        twr.routingSpeakerChannel(TWRClass::TWR_RADIO_TO_SPK);
                    }
                    break;
                default:
                    break;
                }
            }

        } while ( u8g2.nextPage() );

    } while ( btnPressed != LongPress );
    SAVE_CONFIGURE(beep, beep);

}

// void demoDevInfo(uint8_t menuSelect)

Button readButton()
{
    Button btnPressed   = Unknown;
    // Handle key events
    for (uint8_t i = 0; i < COUNT(buttonPins); i++) {
        buttons[i].check();
    }

    // Handling PMU events
    twr.tick();

    if (state != btnPressed) {
        btnPressed = state;
        state = Unknown;
    }
    return btnPressed;
}


uint32_t readRotary(uint32_t cur, uint32_t minOut, uint32_t maxOut, uint32_t steps)
{
    static uint32_t lastMin, lastMax, lastSteps;
    static uint32_t readPotValue;

    if (lastMin != minOut || lastMax != maxOut || lastSteps != steps) {
        setRotaryValue(cur, minOut, maxOut, steps);
        lastMin = minOut;
        lastMax = maxOut;
        lastSteps = steps;
        return cur;
    }

    xQueueReceive(rotaryMsg, &readPotValue, pdMS_TO_TICKS(50));
    return readPotValue;
}

void drawFrame()
{
    u8g2.drawRFrame(0, 0, 128, 64, 5);
}

void drawHeader( uint8_t menuSelect )
{
    u8g2.drawHLine(2, 1, 124);
    u8g2.drawHLine(1, 2, 126);
    u8g2.drawHLine(0, 3, 128);
    u8g2.drawBox(0, 4, 128, 14);

    u8g2.setColorIndex(0);
    u8g2.drawXBMP(4, 1,  16, 16, demo[menuSelect].demo_icon);
    u8g2.setFont(u8g2_font_nokiafc22_tu);
    u8g2.drawStr(22, 12, demo[menuSelect].demo_item);
    u8g2.setColorIndex(1);
}


void beep()
{
    if (twr.isEnableBeep()) {
        tone(ESP32_PWM_TONE, 800, 5);
    }
}


void drawError(uint8_t menuSelect)
{
    Button     btnPressed;
    do {
        btnPressed = readButton();
        u8g2.firstPage();
        do {
            drawFrame();
            drawHeader(menuSelect);
            u8g2.setFont(u8g2_font_nokiafc22_tu);
            u8g2.setCursor(14, 44);
            u8g2.print(F("REV2.0 NOT SUPPORT"));
        } while ( u8g2.nextPage() );
    } while ( btnPressed != LongPress );
}

