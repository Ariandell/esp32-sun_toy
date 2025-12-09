#include "WebPortal.h"
#include <FS.h>
#include <SD.h>

static WebPortal* instance = nullptr;

void WebPortal::begin() {
    instance = this;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    server = new AsyncWebServer(80);
    setupRoutes();
    server->begin();
}

void WebPortal::loop() {}

bool WebPortal::isClientConnected() {
    return WiFi.softAPgetStationNum() > 0;
}

void WebPortal::onVolumeChange(std::function<void(int)> callback) { _volumeCallback = callback; }
void WebPortal::onLedChange(std::function<void(int, int, int)> callback) { _ledCallback = callback; }
void WebPortal::onUploadComplete(std::function<void()> callback) { _uploadCallback = callback; }

void WebPortal::setupRoutes() {
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if (SD.exists("/web/index.html")) request->send(SD, "/web/index.html", "text/html");
        else request->send(200, "text/plain", "Error: index.html not found");
    });

    server->serveStatic("/style.css", SD, "/web/style.css");
    server->serveStatic("/script.js", SD, "/web/script.js");
    server->serveStatic("/assets", SD, "/background"); 


    server->on("/volume", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (request->hasParam("val")) {
            int val = request->getParam("val")->value().toInt();
            if (_volumeCallback) {
                _volumeCallback(val);
            }
            request->send(200, "text/plain", "OK");
        } else request->send(400);
    });

    server->on("/led", HTTP_GET, [this](AsyncWebServerRequest *request){
        int r = request->hasParam("r") ? request->getParam("r")->value().toInt() : 0;
        int g = request->hasParam("g") ? request->getParam("g")->value().toInt() : 0;
        int b = request->hasParam("b") ? request->getParam("b")->value().toInt() : 0;
        if (_ledCallback) _ledCallback(r, g, b);
        request->send(200, "text/plain", "OK");
    });

    server->on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
    }, handleUpload);

    server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        bool success = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", success ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
        if (success) { delay(1000); ESP.restart(); }
    }, handleUpload);
    
    server->on("/rollback", HTTP_GET, [](AsyncWebServerRequest *request){
        if (Update.canRollBack()) { Update.rollBack(); request->send(200, "text/plain", "OK"); delay(1000); ESP.restart(); }
        else request->send(400, "text/plain", "Fail");
    });
}

void WebPortal::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (request->url() == "/update") {
        if (!index) Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        Update.write(data, len);
        if (final) Update.end(true);
        return;
    }
    
    static File file;
    if (!index) {
        if(!SD.exists("/records")) SD.mkdir("/records");
        file = SD.open(FILE_CUSTOM_STORY, FILE_WRITE);
    }
    if (file) file.write(data, len);
    
    if (final) {
        if(file) file.close();
        if (instance && instance->_uploadCallback) {
            instance->_uploadCallback();
        }
    }
}