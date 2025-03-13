#ifndef ESP32_MQTT_MODULE_H
#define ESP32_MQTT_MODULE_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

typedef std::function<void(const String&, const JsonDocument&)> MQTTMessageCallback;
typedef std::function<void(bool connected)> MQTTConnectionCallback;

class ESP32_MQTT_Module {
public:
    ESP32_MQTT_Module(const char* mqtt_server, int mqtt_port = 1883);

    void begin(const char* ssid, const char* password);
    void loop();

    bool publish(const char* topic, const JsonDocument& message, bool retained = false);
    bool subscribe(const char* topic, MQTTMessageCallback callback);
    void onConnectionStateChange(MQTTConnectionCallback callback);
    bool isConnected();
    
private:
    WiFiClient espClient;
    PubSubClient client;
    const char* mqtt_server;
    int mqtt_port;

    void reconnect();
    static void internalCallback(char* topic, byte* payload, unsigned int length);

    static MQTTMessageCallback userCallback;
    MQTTConnectionCallback connectionCallback;
};

#endif