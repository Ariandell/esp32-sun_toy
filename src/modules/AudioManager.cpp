#include "AudioManager.h"
#include "Config.h"
#include <driver/i2s.h> 

void AudioManager::begin() {
    _startSD();
}

void AudioManager::loop() {
    if (_currentMode == MODE_SD && _sdPlayer) {
        _sdPlayer->loop();
    }
}

void AudioManager::setVolume(uint8_t vol) {
    if (_sdPlayer) _sdPlayer->setVolume(vol);
}

void AudioManager::stopSong() {
    if (_sdPlayer && _sdPlayer->isRunning()) {
        _sdPlayer->stopSong(); 
    }
}

void AudioManager::playFile(String path) {
    if (_currentMode == MODE_BLUETOOTH) toggleMode(); 
    stopSong(); 
    delay(50); 
    
    if (_sdPlayer) _sdPlayer->connecttoFS(SD, path.c_str());
}

void AudioManager::playFolder(String folder) {
    if (_currentMode == MODE_BLUETOOTH) toggleMode();
    stopSong();
    delay(50);

    _playlist.clear();
    _currentTrackIndex = 0;

    File dir = SD.open(folder);
    if (!dir || !dir.isDirectory()) return;

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            String name = String(entry.name());
            if (!name.startsWith(".") && (name.endsWith(".wav") || name.endsWith(".mp3"))) {
                _playlist.push_back(folder + "/" + name);
            }
        }
        entry.close();
    }
    
    if (_playlist.size() > 0) {
        playFile(_playlist[0]); 
    }
}

void AudioManager::playNext() {
    if (_playlist.empty()) return;
    stopSong();
    delay(50); 

    _currentTrackIndex++;
    if (_currentTrackIndex >= _playlist.size()) _currentTrackIndex = 0; 
    playFile(_playlist[_currentTrackIndex]);
}

void AudioManager::pauseResume() {
    if (_currentMode == MODE_SD && _sdPlayer) {
        _sdPlayer->pauseResume();
    }
}

void AudioManager::toggleMode() {
    if (_currentMode == MODE_SD) {
        stopSong(); 
        delay(50);
        _stopSD();
        _startBluetooth();
        _currentMode = MODE_BLUETOOTH;
    } else {
        _stopBluetooth();
        _startSD();
        _currentMode = MODE_SD;
    }
}

void AudioManager::_startSD() {
    if (!_sdPlayer) {
        _sdPlayer = new Audio();
        _sdPlayer->setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    }
}

void AudioManager::_stopSD() {
    if (_sdPlayer) { delete _sdPlayer; _sdPlayer = nullptr; }
}

void AudioManager::_startBluetooth() {
    if (!_btPlayer) {
        _btPlayer = new BluetoothA2DPSink();
        i2s_pin_config_t pin_config = {
            .bck_io_num = PIN_I2S_BCLK,
            .ws_io_num = PIN_I2S_LRC,
            .data_out_num = PIN_I2S_DOUT,
            .data_in_num = I2S_PIN_NO_CHANGE
        };
        _btPlayer->set_pin_config(pin_config);
        _btPlayer->start("SunToy Speaker");
    }
}

void AudioManager::_stopBluetooth() {
    if (_btPlayer) { _btPlayer->end(); delete _btPlayer; _btPlayer = nullptr; }
}

bool AudioManager::isPlaying() {
    if (_currentMode == MODE_SD && _sdPlayer) return _sdPlayer->isRunning();
    return (_currentMode == MODE_BLUETOOTH) && (_btPlayer != nullptr);
}