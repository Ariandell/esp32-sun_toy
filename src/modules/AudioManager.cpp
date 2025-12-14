#include "AudioManager.h"
#include <driver/i2s.h>

#define DEBUG_AUDIO 1

#if DEBUG_AUDIO
    #define LOG_AUDIO(msg) Serial.println("[AUDIO] " msg)
    #define LOG_AUDIO_F(fmt, ...) Serial.printf("[AUDIO] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_AUDIO(msg)
    #define LOG_AUDIO_F(fmt, ...)
#endif

AudioManager::AudioManager() : _currentVolume(10), _trackIndex(0), _isBtMode(false), _btInitialized(false), _stateCallback(nullptr) {
    LOG_AUDIO("Constructor called");
}

void AudioManager::begin() {
    LOG_AUDIO("begin() - Initializing I2S audio");
    _prefs.begin("audio", false);
    _currentVolume = _prefs.getInt("volume", 10); 
    LOG_AUDIO_F("Loaded volume: %d", _currentVolume);
    
    _audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    _audio.setVolume(_currentVolume);
    LOG_AUDIO("begin() - Complete");
}

void AudioManager::loop() {
    if (!_isBtMode) {
        _audio.loop();
    }
}

void AudioManager::onStateChange(AudioStateCallback cb) {
    _stateCallback = cb;
}

void AudioManager::notifyStateChange(bool isPlaying) {
    if (_stateCallback) {
        _stateCallback(isPlaying);
    }
}

void AudioManager::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 21) volume = 21;
    
    _currentVolume = volume;
    _prefs.putInt("volume", _currentVolume);
    LOG_AUDIO_F("Volume saved: %d", _currentVolume);
    
    if (_isBtMode && _btInitialized) {
        int btVol = map(_currentVolume, 0, 21, 0, 127);
        _a2dp_sink.set_volume(btVol);
    } else if (!_isBtMode) {
        _audio.setVolume(_currentVolume);
    }
}

int AudioManager::getVolume() {
    return _currentVolume;
}

void AudioManager::playFile(String filename) {
    if (_isBtMode) return;
    
    if (_audio.isRunning()) {
        fadeOut(50);
        _audio.stopSong();
        clearBuffer();
        delay(20);
    }
    
    _audio.setVolume(_currentVolume);  // Restore volume after fadeout
    
    if (!filename.startsWith("/")) filename = "/" + filename;
    LOG_AUDIO_F(" Playing: %s", filename.c_str());
    _audio.connecttoFS(SD, filename.c_str());
    
    notifyStateChange(true); 
}

void AudioManager::playFolder(String folderPath) {
    if (_isBtMode) return;
    
    if (_audio.isRunning()) {
        _audio.stopSong();
    }
    
    loadPlaylist(folderPath);
    if (_playlist.size() > 0) {
        _trackIndex = 0;
        playFile(_playlist[_trackIndex]);
    }
}

void AudioManager::playNext() {
    if (_isBtMode || _playlist.size() == 0) return;
    
    _trackIndex++;
    if (_trackIndex >= _playlist.size()) {
        _trackIndex = 0;
    }
    playFile(_playlist[_trackIndex]);
}

void AudioManager::stop() {
    if (!_isBtMode) {
        _audio.stopSong();
        notifyStateChange(false); 
    }
}

void AudioManager::fadeOut(int durationMs) {
    if (_isBtMode || !_audio.isRunning()) return;
    
    int startVol = _currentVolume;
    int steps = 10;
    int stepDelay = durationMs / steps;
    
    for (int i = startVol; i >= 0; i -= max(1, startVol / steps)) {
        _audio.setVolume(i);
        delay(stepDelay);
    }
    _audio.setVolume(0);
}

void AudioManager::clearBuffer() {
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void AudioManager::pause() {
    if (_isBtMode && _btInitialized) {
        if (_a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
            _a2dp_sink.pause();
            notifyStateChange(false);
        }
    } else if (!_isBtMode) {
        if (_audio.isRunning()) {
            _audio.pauseResume();
            notifyStateChange(false);
        }
    }
}

void AudioManager::resume() {
    if (_isBtMode && _btInitialized) {
        if (_a2dp_sink.get_audio_state() != ESP_A2D_AUDIO_STATE_STARTED) {
            _a2dp_sink.play();
            notifyStateChange(true);
        }
    } else if (!_isBtMode) {
        if (!_audio.isRunning()) {
            _audio.pauseResume();
            notifyStateChange(true);
        }
    }
}

void AudioManager::togglePause() {
    bool newState = false;
    
    if (_isBtMode && _btInitialized) {
        if (_a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
            _a2dp_sink.pause();
            newState = false;
        } else {
            _a2dp_sink.play();
            newState = true;
        }
    } else if (!_isBtMode) {
        _audio.pauseResume(); 
        newState = _audio.isRunning();
    }
    
    notifyStateChange(newState);
}

bool AudioManager::isPlaying() {
    if (_isBtMode && _btInitialized) {
        return (_a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED);
    } else if (_isBtMode) {
        return false;
    } else {
        return _audio.isRunning();
    }
}

void AudioManager::startBluetooth() {
    if (_isBtMode) return;
    
    _audio.stopSong();
    delay(100);
    i2s_driver_uninstall(I2S_NUM_0);
    delay(100);
    
    _isBtMode = true;
    
    i2s_pin_config_t my_pin_config = {
        .bck_io_num = PIN_I2S_BCLK,
        .ws_io_num = PIN_I2S_LRC,
        .data_out_num = PIN_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    _a2dp_sink.set_pin_config(my_pin_config);
    _a2dp_sink.start("SunToy Speaker");
    
    delay(500);
    _btInitialized = true;
    setVolume(_currentVolume);
}

void AudioManager::stopBluetooth() {
    if (!_isBtMode) return;
    
    LOG_AUDIO("BT exit - saving flag and restarting");
    Preferences exitPrefs;
    exitPrefs.begin("audio", false);
    exitPrefs.putBool("bt_exit", true);
    exitPrefs.end();
    
    _btInitialized = false;
    _a2dp_sink.disconnect();
    delay(200);
    
    ESP.restart();
}

BluetoothA2DPSink* AudioManager::getBtSink() {
    if (!_btInitialized) return nullptr;
    return &_a2dp_sink;
}

void AudioManager::loadPlaylist(String folder) {
    _playlist.clear();
    if (!SD.exists(folder)) return;

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
            }
        }
        file = dir.openNextFile();
    }
    dir.close();
}