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

AudioManager::AudioManager() : _currentVolume(10), _trackIndex(0), _isBtMode(false), _btInitialized(false) {
    LOG_AUDIO("Constructor called");
}

void AudioManager::begin() {
    LOG_AUDIO("begin() - Initializing I2S audio");
    LOG_AUDIO_F("  Pins: BCLK=%d, LRC=%d, DOUT=%d", PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    _audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    _audio.setVolume(_currentVolume);
    LOG_AUDIO_F("  Volume set to: %d", _currentVolume);
    LOG_AUDIO("begin() - Complete");
}

void AudioManager::loop() {
    if (!_isBtMode) {
        _audio.loop();
    }
}

void AudioManager::setVolume(int volume) {
    LOG_AUDIO_F("setVolume(%d) - isBtMode=%d, btInit=%d", volume, _isBtMode, _btInitialized);
    if (volume < 0) volume = 0;
    if (volume > 21) volume = 21;
    
    _currentVolume = volume;
    
    if (_isBtMode && _btInitialized) {
        int btVol = map(_currentVolume, 0, 21, 0, 127);
        LOG_AUDIO_F("  Setting BT volume: %d", btVol);
        _a2dp_sink.set_volume(btVol);
    } else if (!_isBtMode) {
        LOG_AUDIO_F("  Setting Audio volume: %d", _currentVolume);
        _audio.setVolume(_currentVolume);
    }
}

int AudioManager::getVolume() {
    return _currentVolume;
}

void AudioManager::playFile(String filename) {
    LOG_AUDIO_F("playFile('%s') - isBtMode=%d", filename.c_str(), _isBtMode);
    if (_isBtMode) {
        LOG_AUDIO("  BLOCKED: In BT mode, ignoring playFile");
        return;
    }
    
    if (_audio.isRunning()) {
        LOG_AUDIO("  Stopping currently playing audio first");
        _audio.stopSong();
        delay(30); 
    }
    
    if (!filename.startsWith("/")) filename = "/" + filename;
    LOG_AUDIO_F("  Playing: %s", filename.c_str());
    _audio.connecttoFS(SD, filename.c_str());
}

void AudioManager::playFolder(String folderPath) {
    LOG_AUDIO_F("playFolder('%s') - isBtMode=%d", folderPath.c_str(), _isBtMode);
    if (_isBtMode) {
        LOG_AUDIO("  BLOCKED: In BT mode, ignoring playFolder");
        return;
    }
    
    if (_audio.isRunning()) {
        LOG_AUDIO("  Stopping currently playing audio first");
        _audio.stopSong();
        delay(30); 
    }
    
    loadPlaylist(folderPath);
    LOG_AUDIO_F("  Playlist loaded: %d tracks", _playlist.size());
    if (_playlist.size() > 0) {
        _trackIndex = 0;
        LOG_AUDIO_F("  Starting track 0: %s", _playlist[_trackIndex].c_str());
        playFile(_playlist[_trackIndex]);
    } else {
        LOG_AUDIO("  WARNING: No tracks found in folder!");
    }
}

void AudioManager::playNext() {
    LOG_AUDIO_F("playNext() - isBtMode=%d, playlistSize=%d, currentIdx=%d", _isBtMode, _playlist.size(), _trackIndex);
    if (_isBtMode) {
        LOG_AUDIO("  BLOCKED: In BT mode");
        return;
    }
    if (_playlist.size() == 0) {
        LOG_AUDIO("  BLOCKED: Empty playlist");
        return;
    }
    
    _trackIndex++;
    if (_trackIndex >= _playlist.size()) {
        _trackIndex = 0;
        LOG_AUDIO("  Wrapped to beginning of playlist");
    }
    LOG_AUDIO_F("  Playing track %d: %s", _trackIndex, _playlist[_trackIndex].c_str());
    playFile(_playlist[_trackIndex]);
}

void AudioManager::stop() {
    LOG_AUDIO_F("stop() - isBtMode=%d", _isBtMode);
    if (!_isBtMode) {
        LOG_AUDIO("  Stopping audio");
        _audio.stopSong();
    } else {
        LOG_AUDIO("  SKIPPED: In BT mode");
    }
}

void AudioManager::pause() {
    LOG_AUDIO_F("pause() - isBtMode=%d, btInit=%d", _isBtMode, _btInitialized);
    if (_isBtMode && _btInitialized) {
        if (_a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
            LOG_AUDIO("  Pausing BT audio");
            _a2dp_sink.pause();
        } else {
            LOG_AUDIO("  BT audio not playing, skip pause");
        }
    } else if (!_isBtMode) {
        if (_audio.isRunning()) {
            LOG_AUDIO("  Pausing SD audio");
            _audio.pauseResume();
        } else {
            LOG_AUDIO("  SD audio not running, skip pause");
        }
    }
}

void AudioManager::resume() {
    LOG_AUDIO_F("resume() - isBtMode=%d, btInit=%d", _isBtMode, _btInitialized);
    if (_isBtMode && _btInitialized) {
        if (_a2dp_sink.get_audio_state() != ESP_A2D_AUDIO_STATE_STARTED) {
            LOG_AUDIO("  Resuming BT audio");
            _a2dp_sink.play();
        }
    } else if (!_isBtMode) {
        if (!_audio.isRunning()) {
            LOG_AUDIO("  Resuming SD audio");
            _audio.pauseResume();
        }
    }
}

void AudioManager::togglePause() {
    LOG_AUDIO_F("togglePause() - isBtMode=%d, btInit=%d", _isBtMode, _btInitialized);
    if (_isBtMode && _btInitialized) {
        if (_a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
            LOG_AUDIO("  Toggle: Pausing BT");
            _a2dp_sink.pause();
        } else {
            LOG_AUDIO("  Toggle: Playing BT");
            _a2dp_sink.play();
        }
    } else if (!_isBtMode) {
        LOG_AUDIO("  Toggle: SD pauseResume");
        _audio.pauseResume();
    } else {
        LOG_AUDIO("  Toggle: SKIPPED - BT mode but not initialized");
    }
}

bool AudioManager::isPlaying() {
    bool playing = false;
    if (_isBtMode && _btInitialized) {
        playing = (_a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED);
    } else if (_isBtMode) {
        playing = false;
    } else {
        playing = _audio.isRunning();
    }
    return playing;
}

void AudioManager::startBluetooth() {
    LOG_AUDIO("========== startBluetooth() ==========");
    LOG_AUDIO_F("  Current state: isBtMode=%d, btInit=%d", _isBtMode, _btInitialized);
    
    if (_isBtMode) {
        LOG_AUDIO("  BLOCKED: Already in BT mode");
        return;
    }
    
    LOG_AUDIO("  Step 1: Stopping any playing audio...");
    _audio.stopSong();
    delay(100);
    
    LOG_AUDIO("  Step 2: Uninstalling I2S driver (I2S_NUM_0)...");
    esp_err_t err = i2s_driver_uninstall(I2S_NUM_0);
    LOG_AUDIO_F("  I2S uninstall result: %d (%s)", err, esp_err_to_name(err));
    delay(100);
    
    LOG_AUDIO("  Step 3: Setting _isBtMode = true");
    _isBtMode = true;
    
    LOG_AUDIO("  Step 4: Configuring A2DP pins...");
    i2s_pin_config_t my_pin_config = {
        .bck_io_num = PIN_I2S_BCLK,
        .ws_io_num = PIN_I2S_LRC,
        .data_out_num = PIN_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    _a2dp_sink.set_pin_config(my_pin_config);
    
    LOG_AUDIO("  Step 5: Starting A2DP Sink 'SunToy Speaker'...");
    _a2dp_sink.start("SunToy Speaker");
    
    LOG_AUDIO("  Step 6: Waiting 500ms for BT init...");
    delay(500);
    
    _btInitialized = true;
    LOG_AUDIO("  Step 7: _btInitialized = true");
    
    int btVol = map(_currentVolume, 0, 21, 0, 127);
    LOG_AUDIO_F("  Step 8: Setting BT volume: %d", btVol);
    _a2dp_sink.set_volume(btVol);
    
    LOG_AUDIO("========== startBluetooth() COMPLETE ==========");
}

void AudioManager::stopBluetooth() {
    LOG_AUDIO("========== stopBluetooth() ==========");
    LOG_AUDIO_F("  Current state: isBtMode=%d, btInit=%d", _isBtMode, _btInitialized);
    
    if (!_isBtMode) {
        LOG_AUDIO("  BLOCKED: Not in BT mode");
        return;
    }
    
    LOG_AUDIO("  Step 1: _btInitialized = false");
    _btInitialized = false;
    
    LOG_AUDIO("  Step 2: Calling _a2dp_sink.end()...");
    _a2dp_sink.end();
    
    LOG_AUDIO("  Step 3: Waiting 1000ms for BT shutdown...");
    delay(1000);
    
    _isBtMode = false;
    LOG_AUDIO("  Step 4: Performing System Reset to clear BT stack...");
    esp_restart();
}

BluetoothA2DPSink* AudioManager::getBtSink() {
    LOG_AUDIO_F("getBtSink() - btInit=%d", _btInitialized);
    if (!_btInitialized) {
        LOG_AUDIO("  Returning nullptr - BT not initialized!");
        return nullptr;
    }
    return &_a2dp_sink;
}

void AudioManager::loadPlaylist(String folder) {
    LOG_AUDIO_F("loadPlaylist('%s')", folder.c_str());
    _playlist.clear();
    
    if (!SD.exists(folder)) {
        LOG_AUDIO("  ERROR: Folder does not exist!");
        return;
    }

    File dir = SD.open(folder);
    if (!dir || !dir.isDirectory()) {
        LOG_AUDIO("  ERROR: Cannot open folder or not a directory!");
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fname = String(folder + "/" + file.name());
            String fnameUpper = fname;
            fnameUpper.toUpperCase();
            if (fnameUpper.endsWith(".MP3") || fnameUpper.endsWith(".WAV")) {
                LOG_AUDIO_F("  Found: %s", fname.c_str());
                _playlist.push_back(fname);
            }
        }
        file = dir.openNextFile();
    }
    dir.close();
    LOG_AUDIO_F("  Total tracks loaded: %d", _playlist.size());
}