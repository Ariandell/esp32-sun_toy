#pragma once
#include <Arduino.h>
#include <Audio.h>
#include <vector>
#include <SD.h>
#include "BluetoothA2DPSink.h"
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
    
    void pause();
    void resume();
    void togglePause();
    
    bool isPlaying();
    
    void setVolume(int volume); 
    int getVolume();
    
    void startBluetooth(); 
    void stopBluetooth();  
    
    BluetoothA2DPSink* getBtSink(); 

private:
    Audio _audio;
    BluetoothA2DPSink _a2dp_sink;
    int _currentVolume;
    std::vector<String> _playlist;
    int _trackIndex;
    bool _isBtMode;
    bool _btInitialized;
    
    void loadPlaylist(String folder);
};