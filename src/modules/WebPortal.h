#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <functional>
#include "Config.h"

class WebPortal {
public:
    void begin();
    void stop();
    void loop();
    bool isClientConnected();
    void onVolumeChange(std::function<void(int)> callback);
    void onLedChange(std::function<void(int, int, int)> callback);
    void onUploadComplete(std::function<void()> callback);

private:
    AsyncWebServer* server = nullptr;
    std::function<void(int)> _volumeCallback;
    std::function<void(int, int, int)> _ledCallback;
    std::function<void()> _uploadCallback;

    void setupRoutes();
    static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
};