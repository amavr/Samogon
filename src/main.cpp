#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <screen.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
BearSSL::CertStore certStore;

#define ONE_WIRE_BUS 0
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor;
int numberOfDevices = 0;
int step = 2;
float t = 0;
float last_t = 0;
bool ahtungSent = false;
float maxT = 82.0;
bool offSent = false;
float offT = 100.0;

uint16_t sizeEEPROM = 512;
uint64_t beg_time = 0;
static char xtime[10];

#include <WiFiController.h>
WiFiController ctrl;

#include <FastBot.h>
FastBot bot;

#include <ESP8266WiFi.h>
WiFiClient wclient;

#include <PubSubClient.h>

WiFiClientSecure espClient;
PubSubClient * cliMQTT;
// PubSubClient cliMQTT(wclient);

const char *MQTT_HOST = "d1ec7bc1e6514d2e8aff3f03c0b50c99.s2.eu.hivemq.cloud";
const uint16_t MQTT_PORT = 8883;

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

    uint16_t ot = offT;
    addr += sizeof(uint16_t);
    EEPROM.get(addr, ot);
    offT = ot;

    addr += sizeof(uint16_t);
    EEPROM.get(addr, step);
}

void saveMaxT()
{
    uint16_t addr = 2 + ctrl.useEEPROMSize() + sizeof(botCfg);
    EEPROM.put(addr, (uint16_t) int(maxT));
    EEPROM.commit();
}

void saveOffT()
{
    uint16_t addr = 2 + ctrl.useEEPROMSize() + sizeof(botCfg) + sizeof(uint16_t);
    EEPROM.put(addr, (uint16_t) int(offT));
    EEPROM.commit();
}

void saveStep()
{
    uint16_t addr = 2 + ctrl.useEEPROMSize() + sizeof(botCfg) + sizeof(uint16_t) + sizeof(uint16_t);
    EEPROM.put(addr, step);
    EEPROM.commit();
}

void saveCfg()
{
    uint16_t addr = ctrl.useEEPROMSize() + 2;
    EEPROM.put(addr, botCfg);

    addr += sizeof(botCfg);
    EEPROM.put(addr, (uint16_t) int(maxT));
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
    else if (msg.text.startsWith("/off"))
    {
        if (msg.text.length() > 5)
        {
            String s = msg.text.substring(5);
            s.trim();
            maxT = s.toFloat();
            saveOffT();
            bot.sendMessage("Выключение при " + String(offT) + "°");
        }
        else
        {
            bot.sendMessage("Не понял команду. Правильно так: /off 101");
        }
    }
    else if (msg.text.startsWith("/step"))
    {
        if (msg.text.length() > 6)
        {
            String s = msg.text.substring(6);
            s.trim();
            step = (uint16_t)s.toInt();
            saveStep();
            bot.sendMessage("Уведомление при изменении на " + String(step) + "°");
        }
        else
        {
            bot.sendMessage("Не понял команду. Правильно так: /step 5");
        }
    }
    else
    {
        bot.sendMessage("Команды:\n/state - получить текущую темп-ру\n/alert 82 - прислать предупреждение при 82°\n/off 101 - выключение при 101°\n/step 5 - уведомление при изменнии на 5°");
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
        last_t = t;
    }
}

void offElectro()
{
}

void onMqttMsg(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (uint16_t i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void reconnect()
{
    // Loop until we’re reconnected
    while (!cliMQTT->connected())
    {
        Serial.print("Attempting MQTT connection…");
        String clientId = "ESP8266Client";
        // Attempt to connect
        // Insert your password
        if (cliMQTT->connect(clientId.c_str(), "samogon", "sam0g0n$"))
        {
            Serial.println("connected");
            // Once connected, publish an announcement…
            cliMQTT->publish("log", "connected");
            // … and resubscribe
            cliMQTT->subscribe("action");
        }
        else
        {
            Serial.print("failed, rc = ");
            Serial.print(cliMQTT->state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void toTime()
{
    uint64_t cur = millis() - beg_time;
    uint16_t ss = cur / 1000;
    uint16_t mm = ss / 60;
    uint16_t hh = mm / 60;
    ss = ss - mm * 60;
    mm = mm - hh * 60;
    String s = (hh < 10 ? "0" : "") + String(hh) + ":" + (mm < 10 ? "0" : "") + String(mm);
    // + ":" + (ss < 10 ? "0" : "") + String(ss);
    strcpy(xtime, s.c_str());
}

void setup()
{
    Serial.begin(115200); // Скорость передачи 115200
    delay(1000);

    EEPROM.begin(sizeEEPROM);
    LittleFS.begin();
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
        bot.sendMessage("Бот запущен. Сейчас " + String(t) + "°. Ахтунг при " + String(maxT) + "°");
    }
    beg_time = millis();

    // int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    // Serial.printf("Number of CA certs read: %d\n", numCerts);
    // if (numCerts == 0)
    // {
    //     Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
    //     return; // Can't connect to anything w/o certs!
    // }

    // BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
    // // Integrate the cert store with this connection
    // bear->setCertStore(&certStore);
    // cliMQTT = new PubSubClient(*bear);


    espClient.setInsecure();    
    cliMQTT = new PubSubClient(espClient);

    cliMQTT->setServer(MQTT_HOST, MQTT_PORT);
    cliMQTT->setCallback(onMqttMsg);
    cliMQTT->subscribe("home/lamp1", 0);
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

        String stemp = String(t);
        String s2 = String(offT) + "/" + String(step);
        toTime();
        showInfo(stemp.c_str(), s2.c_str(), xtime);

        if (!offSent && t >= offT)
        {
            offElectro();
            bot.sendMessage("Аппарат отключен!");
            offSent = true;
        }
        else
        {
            offSent = false;
        }

        if (!ahtungSent && t > maxT)
        {
            ahtungSent = true;
            bot.sendMessage("Ахтунг! Температура " + String(t) + "°");
        }
        else if (t < maxT)
        {
            ahtungSent = false;
        }

        if (abs(t - last_t) > step)
        {
            last_t = t;
            bot.sendMessage("Сейчас: " + String(t) + "°");
        }
    }

    if (!cliMQTT->connected())
    {
        reconnect();
    }
    cliMQTT->loop();

    delay(10);
}