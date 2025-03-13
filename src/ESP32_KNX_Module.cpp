#include "ESP32_KNX_Module.h"

KNXMessageCallback ESP32_KNX_Module::userCallback = nullptr;
KNXRawTelegramCallback ESP32_KNX_Module::rawTelegramCallback = nullptr;

ESP32_KNX_Module::ESP32_KNX_Module(uint8_t rxPin, uint8_t txPin, uint16_t physicalAddress) {
    _rxPin = rxPin;
    _txPin = txPin;
    _physicalAddress = physicalAddress;
}

void ESP32_KNX_Module::begin() {
    // Initialize the KNX library
    address_t addr;
    addr.value = _physicalAddress;
    knx.physical_address_set(addr);
    knx.start();
    
    // Register callback with a name
    knx.callback_register("knx_message", telegramCallback, nullptr);
}

void ESP32_KNX_Module::loop() {
    // Nothing to do here as the library handles events internally
}

bool ESP32_KNX_Module::sendGroupTelegram(uint16_t groupAddress, uint8_t* payload, uint8_t length) {
    // Send a group telegram using the KNX library
    address_t ga;
    ga.value = groupAddress;
    
    // Use the appropriate write method based on payload length
    if (length == 1) {
        knx.write_1bit(ga, payload[0] != 0);
        return true;
    } else {
        // Handle other payload lengths if needed
        return false;
    }
}

void ESP32_KNX_Module::onMessageReceived(KNXMessageCallback callback) {
    userCallback = callback;
}

void ESP32_KNX_Module::onRawTelegramReceived(KNXRawTelegramCallback callback) {
    rawTelegramCallback = callback;
}

bool ESP32_KNX_Module::isConnected() {
    // Return true if KNX is initialized and connected
    // You may need to adapt this based on your library's API
    return true; // Placeholder - implement actual check
}

// Update telegramCallback to also call rawTelegramCallback if set
void ESP32_KNX_Module::telegramCallback(message_t const &msg, void *arg) {
    // Call raw telegram callback if set
    if (rawTelegramCallback) {
        rawTelegramCallback(msg.data, msg.data_len);
    }
    
    if (userCallback) {
        uint16_t groupAddr = msg.received_on.value;
        uint8_t payload[1] = {msg.data[0]};
        userCallback(groupAddr, payload, 1);
    }
}