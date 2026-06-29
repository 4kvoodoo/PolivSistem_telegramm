// Основной файл
#include <ESP8266WiFi.h>
#include <GyverHub.h>
#include "secrets.h"

GyverHub hub("MyDevices", "PolivSistem", "");

void build(gh::Builder& b) {
   b.Title("Hello World");
}


void setup() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    hub.mqtt.config(MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS);

    hub.onBuild(build); // подключаем билдер
    hub.begin();        // запускаем систему
}

void loop() {
    hub.tick();         // тикаем тут
}