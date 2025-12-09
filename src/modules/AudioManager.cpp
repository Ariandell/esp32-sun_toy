#include "AudioManager.h"

AudioManager::AudioManager() : _currentVolume(10), _trackIndex(0) {}

void AudioManager::begin() {
    _audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    _audio.setVolume(_currentVolume);
}

void AudioManager::loop() {
    _audio.loop();
}

void AudioManager::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 21) volume = 21;
    
    _currentVolume = volume;
    _audio.setVolume(_currentVolume);
}

int AudioManager::getVolume() {
    return _currentVolume;
}

void AudioManager::playFile(String filename) {
    if (!filename.startsWith("/")) filename = "/" + filename;
    
    Serial.print("Playing file: "); Serial.println(filename);
    _audio.connecttoFS(SD, filename.c_str());
}

void AudioManager::playFolder(String folderPath) {
    loadPlaylist(folderPath);
    if (_playlist.size() > 0) {
        _trackIndex = 0;
        playFile(_playlist[_trackIndex]);
    } else {
        Serial.println("Folder is empty or not found!");
    }
}

void AudioManager::playNext() {
    if (_playlist.size() == 0) return;
    
    _trackIndex++;
    if (_trackIndex >= _playlist.size()) {
        _trackIndex = 0; 
    }
    playFile(_playlist[_trackIndex]);
}

void AudioManager::stop() {
    _audio.stopSong();
}

bool AudioManager::isPlaying() {
    return _audio.isRunning();
}

void AudioManager::startBluetooth() {
}

void AudioManager::stopBluetooth() {
}

void AudioManager::loadPlaylist(String folder) {
    _playlist.clear();
    if (!SD.exists(folder)) {
        Serial.println("Folder does not exist: " + folder);
        return;
    }

    File dir = SD.open(folder);
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fname = String(folder + "/" + file.name());
            String fnameUpper = fname;
            fnameUpper.toUpperCase();
            if (fnameUpper.endsWith(".MP3") || fnameUpper.endsWith(".WAV")) {
                _playlist.push_back(fname);
                Serial.print("Added to playlist: "); Serial.println(fname);
            }
        }
        file = dir.openNextFile();
    }
}