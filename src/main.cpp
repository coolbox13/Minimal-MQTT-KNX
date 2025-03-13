#include <Arduino.h>
#include "ESP32_MQTT_Module.h"
#include "ESP32_KNX_Module.h"

// WiFi credentials
const char* ssid = "coolbox_down";
const char* password = "1313131313131";

// MQTT settings
const char* mqtt_server = "192.168.178.32";
ESP32_MQTT_Module mqtt(mqtt_server);

// KNX configuration
#define KNX_RX_PIN GPIO_NUM_16
#define KNX_TX_PIN GPIO_NUM_17

// KNX Physical Address configuration
#define KNX_AREA 1      // 1-15
#define KNX_LINE 1      // 0-15
#define KNX_DEVICE 160  // 0-255

// Physical address format: Area.Line.Device (1.1.160)
// - Area: 1-15 (4 bits)
// - Line: 0-15 (4 bits)
// - Device: 0-255 (8 bits)
#define KNX_PHYSICAL_ADDRESS ((KNX_AREA << 12) | (KNX_LINE << 8) | KNX_DEVICE)  // 1.1.160 = 0x11A0 in hex

ESP32_KNX_Module knxModule(KNX_RX_PIN, KNX_TX_PIN, KNX_PHYSICAL_ADDRESS);

// Helper: KNX Group Address builder (main/middle/sub)
// Group address format: Main/Middle/Sub
// - Main: 0-31 (5 bits)
// - Middle: 0-7 (3 bits)
// - Sub: 0-255 (8 bits)
// Example: 0/0/1, 1/2/3, etc.
uint16_t buildGroupAddress(uint8_t main, uint8_t middle, uint8_t sub) {
    // Convert plain numbers to the hex format required by KNX
    return (main << 11) | (middle << 8) | sub;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- ESP32 KNX-MQTT Bridge Starting ---");
    
    // Print ESP32 information
    Serial.print("ESP32 Chip Model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("ESP32 SDK Version: ");
    Serial.println(ESP.getSdkVersion());
    Serial.print("ESP32 Free Heap: ");
    Serial.println(ESP.getFreeHeap());
    
    // Connect WiFi & MQTT
    Serial.println("Connecting to WiFi and MQTT...");
    Serial.print("WiFi SSID: ");
    Serial.println(ssid);
    Serial.print("MQTT Server: ");
    Serial.println(mqtt_server);
    
    mqtt.begin(ssid, password);
    Serial.println("WiFi and MQTT connection initialized");
    
    // Add MQTT connection status monitoring
    mqtt.onConnectionStateChange([](bool connected) {
        if (connected) {
            Serial.println("MQTT: Connected to broker at " + String(mqtt_server));
        } else {
            Serial.println("MQTT: Disconnected from broker");
        }
    });
    
    // Add raw MQTT message monitoring for all topics
    mqtt.subscribe("#", [](const String& topic, const JsonDocument& doc) {
        Serial.println("DEBUG: Raw MQTT message received on topic: " + topic);
        String payload;
        serializeJson(doc, payload);
        Serial.println("DEBUG: MQTT payload: " + payload);
    });
    
    mqtt.subscribe("home/knx/control", [](const String& topic, const JsonDocument& doc) {
        Serial.println("Received MQTT message on topic: " + topic);
        
        // Replace deprecated containsKey with is<JsonObject>() check
        if (!doc["value"].is<int>()) {
            Serial.println("ERROR: MQTT message missing 'value' field or not an integer");
            return;
        }
        
        uint8_t payload = doc["value"];
        // Using group address 0/0/1 for this example
        // This could represent a specific function like a light switch
        uint16_t ga = buildGroupAddress(0, 0, 1);  // Example GA: 0/0/1
        
        Serial.printf("Sending to KNX: Group Address 0/0/1, Value: %d\n", payload);
        bool success = knxModule.sendGroupTelegram(ga, &payload, 1);
        
        if (success) {
            Serial.printf("SUCCESS: MQTT->KNX: Sent value %d to GA 0/0/1\n", payload);
        } else {
            Serial.printf("ERROR: Failed to send value %d to KNX GA 0/0/1\n", payload);
        }
    });

    // KNX setup
    Serial.printf("Initializing KNX with physical address %d.%d.%d (0x%04X)...\n", 
                 KNX_AREA, KNX_LINE, KNX_DEVICE, KNX_PHYSICAL_ADDRESS);
    knxModule.begin();
    Serial.println("KNX module initialized");
    
    // Add raw KNX telegram monitoring
    knxModule.onRawTelegramReceived([](uint8_t* telegram, uint16_t length) {
        Serial.print("DEBUG: Raw KNX telegram received: ");
        for (int i = 0; i < length; i++) {
            Serial.printf("%02X ", telegram[i]);
        }
        Serial.println();
    });
    
    knxModule.onMessageReceived([](uint16_t groupAddr, uint8_t* payload, uint8_t length) {
        uint8_t main = (groupAddr >> 11) & 0x1F;
        uint8_t middle = (groupAddr >> 8) & 0x07;
        uint8_t sub = groupAddr & 0xFF;
        
        Serial.printf("KNX Message Received: GA: %d/%d/%d (0x%04X), Payload: %d, Length: %d\n",
            main, middle, sub, groupAddr, payload[0], length);

        // Forward KNX message to MQTT
        Serial.println("Forwarding KNX message to MQTT...");
        JsonDocument json;
        json["groupAddress"] = String(main) + "/" + String(middle) + "/" + String(sub);
        json["value"] = payload[0];
        
        bool published = mqtt.publish("home/knx/status", json);
        if (published) {
            Serial.println("SUCCESS: KNX->MQTT: Message forwarded to topic home/knx/status");
        } else {
            Serial.println("ERROR: Failed to publish KNX message to MQTT");
        }
    });
    
    // Send a test message to MQTT to verify connection
    Serial.println("Attempting to send test message to MQTT...");
    JsonDocument testMsg;
    testMsg["status"] = "online";
    testMsg["device"] = "ESP32_KNX_Bridge";
    if (mqtt.publish("home/knx/bridge/status", testMsg)) {
        Serial.println("Test message published to MQTT");
    } else {
        Serial.println("Failed to publish test message to MQTT");
        Serial.println("Checking MQTT connection status...");
        if (mqtt.isConnected()) {
            Serial.println("MQTT reports as connected, but publish failed");
        } else {
            Serial.println("MQTT is not connected");
        }
    }
    
    Serial.println("Setup complete - Bridge is running");
}

void loop() {
    mqtt.loop();
    knxModule.loop();
    
    // Add a heartbeat message every 30 seconds
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {
        Serial.println("Heartbeat: KNX-MQTT Bridge running");
        
        // Check and report connection status
        Serial.println("Status: MQTT " + String(mqtt.isConnected() ? "connected" : "disconnected"));
        Serial.println("Status: KNX " + String(knxModule.isConnected() ? "connected" : "disconnected"));
        
        lastHeartbeat = millis();
    }
}