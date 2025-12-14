#pragma once
#include <Arduino.h>
#include <Audio.h>
#include <vector>
#include <SD.h>
#include <Preferences.h>
#include "BluetoothA2DPSink.h"
#include "Config.h"

typedef std::function<void(bool)> AudioStateCallback;

class AudioManager {
public:
    AudioManager();
    void begin();
    void loop();
    
    void playFile(String filename);
    void playFolder(String folderPath);
    void playNext();
    void stop();
    void fadeOut(int durationMs = 100);
    void clearBuffer();
    
    void pause();
    void resume();
    void togglePause();
    
    bool isPlaying();
    
    void setVolume(int volume); 
    int getVolume();
    
    void startBluetooth(); 
    void stopBluetooth();  
    
    BluetoothA2DPSink* getBtSink(); 
    void onStateChange(AudioStateCallback cb);

private:
    Audio _audio;
    BluetoothA2DPSink _a2dp_sink;
    int _currentVolume;
    std::vector<String> _playlist;
    int _trackIndex;
    bool _isBtMode;
    bool _btInitialized;
    AudioStateCallback _stateCallback;
    Preferences _prefs;
    
    void loadPlaylist(String folder);
    void notifyStateChange(bool isPlaying);
};