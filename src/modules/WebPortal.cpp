#include "WebPortal.h"
#include <FS.h>
#include <SD.h>
#include <Update.h>
#include "AudioManager.h"
#include "LedController.h"

extern AudioManager audioManager;
extern LedController led;

void WebPortal::begin() {
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

void WebPortal::setupRoutes() {
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if (SD.exists("/web/index.html")) {
            request->send(SD, "/web/index.html", "text/html");
        } else {
            request->send(200, "text/plain", "Error: index.html not found on SD/web/");
        }
    });

    server->serveStatic("/style.css", SD, "/web/style.css");
    server->serveStatic("/script.js", SD, "/web/script.js");

    server->serveStatic("/assets", SD, "/background"); 

    server->on("/volume", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("val")) {
            audioManager.setVolume(request->getParam("val")->value().toInt());
            request->send(200, "text/plain", "OK");
        } else request->send(400);
    });

    server->on("/bt-mode", HTTP_POST, [](AsyncWebServerRequest *request){
        audioManager.toggleMode();
        request->send(200, "text/plain", "OK");
    });

    server->on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
        int r = request->hasParam("r") ? request->getParam("r")->value().toInt() : 0;
        int g = request->hasParam("g") ? request->getParam("g")->value().toInt() : 0;
        int b = request->hasParam("b") ? request->getParam("b")->value().toInt() : 0;
        led.setColor(r, g, b);
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
        if (success) {
            delay(1000);
            ESP.restart();
        }
    }, handleUpload);
    
    server->on("/rollback", HTTP_GET, [](AsyncWebServerRequest *request){
        if (Update.canRollBack()) {
            Update.rollBack();
            request->send(200, "text/plain", "OK");
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "text/plain", "Fail");
        }
    });
}

void WebPortal::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (request->url() == "/update") {
        if (!index) {
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                Update.printError(Serial);
            }
        }
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
        if (final) {
            if (Update.end(true)) Serial.printf("Update Success: %u B\n", index+len);
            else Update.printError(Serial);
        }
        return;
    }
    static File file;
    if (!index) {
        Serial.printf("Upload Start: %s\n", filename.c_str());
        if(!SD.exists("/records")) SD.mkdir("/records");
        file = SD.open(FILE_CUSTOM_STORY, FILE_WRITE);
    }
    if (file) file.write(data, len);
    if (final) {
        if(file) file.close();
        Serial.printf("Upload End: %u B\n", index+len);
        audioManager.playFile(FILE_CUSTOM_STORY); 
    }
}