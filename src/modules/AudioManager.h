#pragma once
#include <Arduino.h>
#include <Audio.h>
#include <BluetoothA2DPSink.h>
#include <vector>
#include <SD.h>

enum AudioMode { MODE_SD, MODE_BLUETOOTH };

class AudioManager {
public:
    void begin();
    void loop();
    
    void setVolume(uint8_t vol);
    void playFile(String path);
    void playFolder(String folder);
    void playNext();
    
    void stopSong(); 
    void pauseResume();
    
    void toggleMode(); 
    bool isPlaying();
    AudioMode getMode() { return _currentMode; }

private:
    Audio* _sdPlayer = nullptr;
    BluetoothA2DPSink* _btPlayer = nullptr;
    AudioMode _currentMode = MODE_SD;
    
    std::vector<String> _playlist;
    int _currentTrackIndex = -1;
    
    void _startBluetooth();
    void _stopBluetooth();
    void _startSD();
    void _stopSD();
};