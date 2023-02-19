#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 0
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor;
int numberOfDevices = 0;
float t = 0;
bool alertSent = false;
float maxT = 82.0;

uint16_t sizeEEPROM = 512;

#include <WiFiController.h>
WiFiController ctrl;

#include <FastBot.h>
FastBot bot;

struct TBotCfg
{
    char token[60] = "6274546677:AAEJFpGyYc7LlaBvq8KD5mQiHtvBf2eHx9E"; // 60 - размер токена с запасом
    char chatId[16] = "-874914426";
} botCfg;

void loadCfg()
{
    uint16_t addr = ctrl.useEEPROMSize() + 2;
    EEPROM.get(addr, botCfg);

    uint16_t mt = maxT;
    addr += sizeof(botCfg);
    EEPROM.get(addr, mt);
    maxT = mt;
}

void saveMaxT()
{
    uint16_t addr = 2 + ctrl.useEEPROMSize() + sizeof(botCfg);
    EEPROM.put(addr, (uint16_t)int(maxT));
    EEPROM.commit();
}

void saveCfg()
{
    uint16_t addr = ctrl.useEEPROMSize() + 2;
    EEPROM.put(addr, botCfg);

    addr += sizeof(botCfg);
    EEPROM.put(addr, (uint16_t)int(maxT));
    EEPROM.commit();
}

// сброс настроек: сохранение корректных, но пустых настроек
void reset()
{
    EEPROM[0] = 0xff;
    EEPROM.commit();

    saveCfg();
}

// обработчик сообщений
void onTlgMsg(FB_msg &msg)
{
    //   выводим имя юзера и текст сообщения
    //   Serial.print(msg.username);
    //   Serial.print(", ");
    Serial.println(msg.text);

    //   выводим всю информацию о сообщении
    // Serial.println(msg.toString());
    if (msg.text.equals("/state"))
    {
        bot.sendMessage("Сейчас: " + String(t) + "°. Ахтунг при " + String(maxT) + "°");
    }
    else if (msg.text.startsWith("/alert"))
    {
        if (msg.text.length() > 7)
        {
            String s = msg.text.substring(7);
            s.trim();
            maxT = s.toFloat();
            saveMaxT();
            bot.sendMessage("Ахтунг при " + String(maxT) + "°");
        }
        else
        {
            bot.sendMessage("Не понял команду. Правильно так: /alert 82");
        }
    }
    else{
        bot.sendMessage("Команды:\n/state - получить текущую темп-ру\n/alert 82 - прислать предупреждение при 82°");
    }
}

void initSensor()
{
    sensors.begin();
    numberOfDevices = sensors.getDeviceCount();
    Serial.printf("numberOfDevices: %d\n", numberOfDevices);

    if (oneWire.search(sensor))
    {
        // 28 79 55 81 E3 EE 3C 96
        sensors.requestTemperatures();
        t = sensors.getTempC(sensor);
    }
}

void setup()
{
    Serial.begin(115200); // Скорость передачи 115200
    delay(1000);

    EEPROM.begin(sizeEEPROM);

    // reset();

    // Признак первого запуска
    bool isFirstTime = EEPROM[0] != 0x22;
    Serial.printf("isFirstTime: %s\n", isFirstTime ? "true" : "false");
    if (isFirstTime)
        reset();

    initSensor();

    // подключение к WiFi
    ctrl.connect(isFirstTime);

    if (isFirstTime)
    {
        EEPROM[0] = 0x22;
        EEPROM.commit();
    }

    loadCfg();
    Serial.printf("botCfg: {%s, %s}\n", botCfg.chatId, botCfg.token);
    bot.setToken(botCfg.token);
    bot.setChatID(botCfg.chatId);
    bot.attach(onTlgMsg);
    if (numberOfDevices == 0)
    {
        bot.sendMessage("Бот запущен, но датчика темп-ры не найдено");
    }
    else
    {
        bot.sendMessage("Бот запущен. Сейчас " + String(t) + "°. Ахтунг при "+ String(maxT) + "°");
    }
}

int n = 0;
void loop()
{
    // проверка WiFi и если требуется - подключение
    ctrl.tick();
    // проверка новых сообщений бота
    bot.tick();

    if (numberOfDevices > 0)
    {
        sensors.requestTemperatures();
        t = sensors.getTempC(sensor);
        if(!alertSent && t > maxT)
        {
            alertSent = true;
            bot.sendMessage("Ахтунг! Температура " + String(t) + "°");
        }
        else if(t < maxT)
        {
            alertSent = false;
        }
    }

    delay(10);
}