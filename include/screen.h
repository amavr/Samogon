#include <GyverOLED.h>
GyverOLED<SSD1306_128x64> oled;

// const uint8_t SDA = 3;
// const uint8_t SCL = 5;


void screenInit()
{
    // oled.init(); // инициализация
    oled.init(SDA, SCL); // инициализация
}

void showInfo(const char *bigText, const char *smallText1, const char *smallText2)
{
    oled.clear();
    oled.setScale(3);
    oled.setCursor(0, 0);
    oled.print(bigText);
    oled.setScale(2);
    oled.setCursor(0, 4);
    oled.print(smallText1);
    oled.setScale(2);
    oled.setCursor(0, 6);
    oled.print(smallText2);
    oled.update();
}