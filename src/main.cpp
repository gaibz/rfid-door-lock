/**
 * RFID Door Lock
 * Project Started at : Desember 2020
 * @author : Herlangga Sefani
 */
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <MFRC522.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

// Also known as SDA
#define SS_PIN D4
#define RST_PIN D3
#define UNLOCK_PIN D0
#define LOCK_PIN D1
#define MANUAL_PIN D2
#define PILOT_PIN D8

#define EEPROM_SIZE 512

#define STATUS_LOCKED 1
#define STATUS_UNLOCKED 0

#define WIFI_SSID "namawifi"
#define WIFI_PASS "password-wifi"

#define RFID_MODE_READ 0
#define RFID_MODE_WRITE 1

MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

int lock_status = STATUS_UNLOCKED;
int status_address = 0;
int rfid_mode = RFID_MODE_READ;

// config start here
// local ip address buat esp8266
IPAddress local_ip = IPAddress(192, 168, 0, 53);
IPAddress gateway = IPAddress(192, 168, 0, 1);
IPAddress subnet = IPAddress(255, 255, 255, 0);
IPAddress dns1 = IPAddress(8, 8, 8, 8);
IPAddress dns2 = IPAddress(8, 8, 4, 4);
IPAddress mqtt_server = IPAddress(192,168,0,8);
int mqtt_port = 1883;
// config end here

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
String getByteString(byte *buffer, byte bufferSize)
{
    String data = "";
    for (byte i = 0; i < bufferSize; i++)
    {
        data += buffer[i] < 0x10 ? " 0" : " ";
        data += String(buffer[i], HEX);
    }
    data.toUpperCase();
    return data;
}

/**
 * Save Lock status on EEPROM
 */
void saveLockStatus()
{
    EEPROM.write(status_address, lock_status);
    EEPROM.commit();
}

/**
 * Stop Motor from running
 */
void stop() {
    digitalWrite(LOCK_PIN, LOW);
    digitalWrite(UNLOCK_PIN, LOW);
    saveLockStatus();
}

/**
 * Activate the lock
 */
void lock()
{
    Serial.println("LOCKED");
    digitalWrite(LOCK_PIN, HIGH);
    digitalWrite(UNLOCK_PIN, LOW);
    lock_status = STATUS_LOCKED;
    delay(400);
    stop();
}

/**
 * Unlock the door
 */
void unlock()
{
    Serial.println("UNLOCKED");
    digitalWrite(LOCK_PIN, LOW);
    digitalWrite(UNLOCK_PIN, HIGH);
    lock_status = STATUS_UNLOCKED;
    delay(400);
    stop();
}

/**
 * Incoming message from MQTT
 * @param topic
 * @param message
 * @param length
 */
void onMQTTMessage(char *topic, byte *message, unsigned int length)
{
    //Serial.print("Message arrived on topic: ");
    //Serial.print(topic);
    //Serial.print(". Message: ");
    char _message[30] = "";

    for (uint8_t i = 0; i < length; i++)
    {
        //Serial.print((char)message[i]);
        _message[i] = (char)message[i];
    }
    //Serial.println();

    if (strcmp(topic, "doorlock") == 0)
    {
        if (strcmp(_message, "GRANTED") == 0)
        {
            if (lock_status == STATUS_LOCKED) {
                unlock();
            }
            else
            {
                lock();
            }
        }
    }
}


/**
 * Connect to Cloud MQTT
 */
void connectMQTT()
{
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(onMQTTMessage);

    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (!mqtt.connected())
    {
        if (mqtt.connect("doorlock-esp"))
        {
            //Serial.println("connected to mqtt");
            // Subscribe
            mqtt.subscribe("doorlock");
            mqtt.subscribe("doorlock/insert/status");
        }
    }
}


/**
 * Setup OTA Server for Uploading via OTA
 */
void setupOTAServer()
{
    // Port defaults to 3232
    ArduinoOTA.setPort(8266);

    // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setHostname("doorlock.local");

    // No authentication by default
    ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        }
        else // U_SPIFFS
        {
            type = "filesystem";
        }
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        char otaprogress[17] = "";
        sprintf(otaprogress, "%i %s", (progress / (total / 100)), "%");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        //Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
        {
            //Serial.println("Auth Failed");
        }
        else if (error == OTA_BEGIN_ERROR)
        {
            //Serial.println("Begin Failed");
        }
        else if (error == OTA_CONNECT_ERROR)
        {
            //Serial.println("Connect Failed");
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
            //Serial.println("Receive Failed");
        }
        else if (error == OTA_END_ERROR)
        {
            //Serial.println("End Failed");
        }
    });

    ArduinoOTA.begin();
}

/**
 * Setup & Connect Wifi
 */
void connectWiFi()
{

    // Configures static IP address
    if (!WiFi.config(local_ip, gateway, subnet, dns1, dns2))
    {
        Serial.println("STA Failed to configure");
    }

    Serial.println("Connecting");
    delay(2000);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int try_counter = 0;
    while (WiFi.status() != WL_CONNECTED && try_counter < 10)
    {
        Serial.print('.');
        delay(1000);
        try_counter++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected!");
        setupOTAServer();
        connectMQTT();
    }
    else
    {
        Serial.println("Error!");
    }
}


/**
 * Setup RFID Module
 */
void setupRFID()
{
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, HIGH); // Exit power down mode. This triggers a hard reset.
    delay(50);
    mfrc522.PCD_Init(); // Init MFRC522
    delay(4);           // Optional delay. Some board do need more time after init to be ready, see Readme
    mfrc522.PCD_PerformSelfTest();
    mfrc522.PCD_DumpVersionToSerial();
}

/**
 * Read RFID
 */
void readRFID()
{
    // need to recall this to make sure rfid is running
    //    digitalWrite(RST_PIN, HIGH); // Exit power down mode. This triggers a hard reset.
    mfrc522.PCD_Init();
    delay(4);

    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if (!mfrc522.PICC_IsNewCardPresent())
    {
        //        //Serial.println("No RFID Card present");
        return;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial())
    {
        //        //Serial.println(F("No RFID Serial"));
        return;
    }

    // dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    String data = getByteString(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println("Card UID : " + data);
    if (rfid_mode == RFID_MODE_READ) {
        mqtt.publish("doorlock/read", data.c_str());
    }
    else
    {
        mqtt.publish("doorlock/insert", data.c_str());
    }
}

/**
 * Setup (Needed as using Arduino Framework)
 */
void setup() {
    Serial.begin(9600);
    pinMode(LOCK_PIN, OUTPUT);
    pinMode(UNLOCK_PIN, OUTPUT);
    pinMode(MANUAL_PIN, INPUT);
    pinMode(PILOT_PIN, OUTPUT);
    digitalWrite(LOCK_PIN, LOW);
    digitalWrite(UNLOCK_PIN, LOW);
    SPI.begin();
    setupRFID();
    connectWiFi();
    EEPROM.begin(EEPROM_SIZE);

    lock_status = EEPROM.read(status_address);
    if (lock_status == STATUS_LOCKED) {
        lock();
    }
    else
    {
        unlock();
    }
}

/**
 * The magic starts here ...
 */
void loop() {
    readRFID();

    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
        if(mqtt.connected()) {
            mqtt.loop();
        }
        else
        {
            connectMQTT();
        }
    }
    else
    {
        connectWiFi();
    }

    if (digitalRead(MANUAL_PIN) == HIGH) {
        if (lock_status == STATUS_LOCKED) {
            unlock();
        }
        else
        {
            lock();
        }
        delay(1000);
    }

    if (lock_status == STATUS_LOCKED) {
        digitalWrite(PILOT_PIN, HIGH);
    }
    else
    {
        digitalWrite(PILOT_PIN, LOW);
    }
}

// End Of File main.cpp