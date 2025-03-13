#include "ESP32_MQTT_Module.h"

MQTTMessageCallback ESP32_MQTT_Module::userCallback = nullptr;

ESP32_MQTT_Module::ESP32_MQTT_Module(const char* mqtt_server, int mqtt_port) : client(espClient) {
    this->mqtt_server = mqtt_server;
    this->mqtt_port = mqtt_port;
    connectionCallback = nullptr;
}

void ESP32_MQTT_Module::begin(const char* ssid, const char* password) {
    Serial.println("MQTT Debug: Setting up WiFi connection...");
    WiFi.begin(ssid, password);
    
    Serial.print("MQTT Debug: Waiting for WiFi connection");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nMQTT Debug: WiFi connected!");
    Serial.print("MQTT Debug: IP address: ");
    Serial.println(WiFi.localIP());
    
    Serial.print("MQTT Debug: Setting MQTT server to: ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.println(mqtt_port);
    
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(internalCallback);
    
    // Try to connect immediately
    Serial.println("MQTT Debug: Attempting initial connection...");
    reconnect();
    
    // If connection state changes, call the callback
    if (connectionCallback) {
        connectionCallback(client.connected());
    }
}

void ESP32_MQTT_Module::loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}

void ESP32_MQTT_Module::reconnect() {
    // Try to reconnect
    if (!client.connected()) {
        Serial.print("MQTT Debug: Connecting to MQTT broker at ");
        Serial.print(mqtt_server);
        Serial.print(":");
        Serial.print(mqtt_port);
        Serial.print("...");
        
        // Create a random client ID
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        
        Serial.print(" Using client ID: ");
        Serial.print(clientId);
        
        // Attempt to connect with last will message for status
        bool result = client.connect(
            clientId.c_str(),
            NULL,  // username
            NULL,  // password
            "home/knx/bridge/status",  // will topic
            0,     // will QoS
            true,  // will retain
            "{\"status\":\"offline\",\"device\":\"ESP32_KNX_Bridge\"}"  // will message
        );
        
        if (result) {
            Serial.println(" Connected!");
            
            // Publish online status
            JsonDocument statusMsg;
            statusMsg["status"] = "online";
            statusMsg["device"] = "ESP32_KNX_Bridge";
            statusMsg["ip"] = WiFi.localIP().toString();
            publish("home/knx/bridge/status", statusMsg, true);
            
            // No need to resubscribe here - it's handled in the subscribe method
        } else {
            int state = client.state();
            Serial.print(" Failed, rc=");
            Serial.print(state);
            
            // Move the IPAddress declaration outside the switch statement
            IPAddress mqttIP;
            
            // Detailed error messages
            switch(state) {
                case -4: 
                    Serial.println(" (MQTT_CONNECTION_TIMEOUT)"); 
                    break;
                case -3: 
                    Serial.println(" (MQTT_CONNECTION_LOST)"); 
                    break;
                case -2: 
                    Serial.println(" (MQTT_CONNECT_FAILED)");
                    Serial.print("MQTT Debug: Can ping MQTT server: ");
                    // Try to ping the MQTT server to check connectivity
                    if (WiFi.hostByName(mqtt_server, mqttIP)) {
                        Serial.print("IP resolved to: ");
                        Serial.print(mqttIP);
                        Serial.println(", Hostname resolved successfully");
                    } else {
                        Serial.println("Failed to resolve hostname");
                    }
                    break;
                case -1: 
                    Serial.println(" (MQTT_DISCONNECTED)"); 
                    break;
                case 1: 
                    Serial.println(" (MQTT_CONNECT_BAD_PROTOCOL)"); 
                    break;
                case 2: 
                    Serial.println(" (MQTT_CONNECT_BAD_CLIENT_ID)"); 
                    break;
                case 3: 
                    Serial.println(" (MQTT_CONNECT_UNAVAILABLE)"); 
                    break;
                case 4: 
                    Serial.println(" (MQTT_CONNECT_BAD_CREDENTIALS)"); 
                    break;
                case 5: 
                    Serial.println(" (MQTT_CONNECT_UNAUTHORIZED)"); 
                    break;
                default: 
                    Serial.println(" (unknown error)"); 
                    break;
            }
            Serial.println("MQTT Debug: Retrying in 5 seconds");
            delay(5000);
        }
    }
    
    // If connection state changes, call the callback
    if (connectionCallback) {
        connectionCallback(client.connected());
    }
}

bool ESP32_MQTT_Module::publish(const char* topic, const JsonDocument& message, bool retained) {
    if (!client.connected()) {
        Serial.println("MQTT Debug: Cannot publish - not connected to broker");
        return false;
    }
    
    Serial.print("MQTT Debug: Publishing to topic: ");
    Serial.println(topic);
    
    // Serialize JSON to string
    String jsonString;
    serializeJson(message, jsonString);
    
    Serial.print("MQTT Debug: Message payload: ");
    Serial.println(jsonString);
    
    // Publish
    bool success = client.publish(topic, jsonString.c_str(), retained);
    
    if (success) {
        Serial.println("MQTT Debug: Publish successful");
    } else {
        Serial.println("MQTT Debug: Publish failed");
    }
    
    return success;
}

bool ESP32_MQTT_Module::subscribe(const char* topic, MQTTMessageCallback callback) {
    Serial.print("MQTT Debug: Subscribing to topic: ");
    Serial.println(topic);
    
    userCallback = callback;
    
    if (client.connected()) {
        bool result = client.subscribe(topic);
        if (result) {
            Serial.println("MQTT Debug: Subscription successful");
        } else {
            Serial.println("MQTT Debug: Subscription failed");
        }
        return result;
    } else {
        Serial.println("MQTT Debug: Cannot subscribe - not connected to broker");
        return false;
    }
}

void ESP32_MQTT_Module::onConnectionStateChange(MQTTConnectionCallback callback) {
    connectionCallback = callback;
    
    // Call the callback with the current state if already connected
    if (callback && client.connected()) {
        callback(true);
    }
}

bool ESP32_MQTT_Module::isConnected() {
    return client.connected();
}

void ESP32_MQTT_Module::internalCallback(char* topic, byte* payload, unsigned int length) {
    // Update this to use JsonDocument instead of StaticJsonDocument
    JsonDocument doc;
    deserializeJson(doc, payload, length);
    if (userCallback) {
        userCallback(String(topic), doc);
    }
}