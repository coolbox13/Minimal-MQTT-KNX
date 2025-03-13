#ifndef ESP32_KNX_MODULE_H
#define ESP32_KNX_MODULE_H

#include <Arduino.h>
#include "esp-knx-ip.h"  // correct header for KNX library

typedef std::function<void(uint16_t groupAddr, uint8_t* payload, uint8_t length)> KNXMessageCallback;
typedef std::function<void(uint8_t* telegram, uint16_t length)> KNXRawTelegramCallback;

class ESP32_KNX_Module {
public:
    ESP32_KNX_Module(uint8_t rxPin, uint8_t txPin, uint16_t physicalAddress);

    void begin();
    void loop();

    bool sendGroupTelegram(uint16_t groupAddress, uint8_t* payload, uint8_t length);
    void onMessageReceived(KNXMessageCallback callback);
    void onRawTelegramReceived(KNXRawTelegramCallback callback);
    bool isConnected();

private:
    uint8_t _rxPin;
    uint8_t _txPin;
    uint16_t _physicalAddress;

    static void telegramCallback(message_t const &msg, void *arg);

    static KNXMessageCallback userCallback;
    static KNXRawTelegramCallback rawTelegramCallback;
};

#endif