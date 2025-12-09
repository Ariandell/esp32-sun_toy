#pragma once
#include <Arduino.h>
#include <Audio.h>
#include <vector>
#include <SD.h>
#include "Config.h"

class AudioManager {
public:
    AudioManager();
    void begin();
    void loop();
    void playFile(String filename);
    void playFolder(String folderPath);
    void playNext();
    void stop();
    bool isPlaying();
    void setVolume(int volume); 
    int getVolume();
    void startBluetooth(); 
    void stopBluetooth();  

private:
    Audio _audio;
    int _currentVolume;
    std::vector<String> _playlist;
    int _trackIndex;
    
    void loadPlaylist(String folder);
};