#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "Config.h"

class WebPortal {
public:
    void begin();
    void loop();
    bool isClientConnected();

private:
    AsyncWebServer* server;
    void setupRoutes();
    static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
};