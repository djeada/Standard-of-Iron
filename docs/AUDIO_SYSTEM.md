# Audio System Documentation

## Overview

The Standard of Iron game uses a custom audio system built on top of the miniaudio library. This document describes the architecture, common issues, and troubleshooting steps.

## Architecture

### Components

1. **AudioSystem** (`game/audio/AudioSystem.h/cpp`)
   - Main audio management singleton
   - Handles sound and music playback requests
   - Manages resource loading and unloading
   - Runs audio processing on a dedicated thread

2. **MusicPlayer** (`game/audio/MusicPlayer.h/cpp`)
   - Manages background music playback
   - Supports multiple music channels
   - Handles crossfading between tracks
   - Must be initialized on the GUI thread

3. **MiniaudioBackend** (`game/audio/MiniaudioBackend.h/cpp`)
   - Low-level audio device interface
   - Uses miniaudio library for cross-platform audio
   - Handles PCM decoding and mixing
   - Manages audio callback for real-time playback

4. **Sound** (`game/audio/Sound.h/cpp`)
   - Represents individual sound effects
   - Pre-decodes audio files for low-latency playback
   - Supports looping and volume control

5. **AudioEventHandler** (`game/audio/AudioEventHandler.h/cpp`)
   - Bridges game events to audio playback
   - Handles unit selection sounds
   - Manages ambient music transitions
   - Responds to game state changes

## Asset Requirements

### Directory Structure

Audio assets must be located at:
```
<application_directory>/assets/audio/
├── music/
│   ├── peaceful.wav
│   ├── tense.wav
│   ├── combat.wav
│   ├── victory.wav
│   └── defeat.wav
└── voices/
    ├── archer_voice.wav
    ├── knight_voice.wav
    └── spearman_voice.wav
```

### Build Configuration

The CMakeLists.txt must copy assets to the correct location:
```cmake
# Copy assets next to the binary for dev runs
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/bin/assets/)
```

**Important:** The binary is placed in `${CMAKE_BINARY_DIR}/bin/` (set by `RUNTIME_OUTPUT_DIRECTORY`), so assets must also be copied to `bin/assets/` directory.

### Supported Formats

- **WAV** (always supported, built-in decoder)
- **MP3** (enabled via `MA_ENABLE_MP3`)
- **FLAC** (enabled via `MA_ENABLE_FLAC`)
- **Vorbis/OGG** (enabled via `MA_ENABLE_VORBIS`)

## Initialization Flow

1. **GameEngine constructor** calls `AudioSystem::getInstance().initialize()`
2. **AudioSystem::initialize()** gets MusicPlayer singleton and calls `initialize()`
3. **MusicPlayer::initialize()** creates MiniaudioBackend and initializes it
4. **MiniaudioBackend::initialize()** initializes miniaudio device and starts playback
5. **GameEngine** calls `loadAudioResources()` to load all audio files
6. **AudioEventHandler** is initialized and event subscriptions are set up

## Common Issues and Solutions

### Issue: No Audio Output

**Symptoms:**
- No sound during gameplay
- No background music
- Volume controls appear to work but no audio

**Diagnosis:**
Check logs for:
```
Loading audio resources from: <path>
Audio assets directory does not exist: <path>
Failed to load <asset> from: <path>
MiniaudioBackend: Failed to initialize audio device
```

**Common Causes:**

1. **Assets not in correct location**
   - **Fix:** Ensure CMakeLists.txt copies to `${CMAKE_BINARY_DIR}/bin/assets/`
   - Verify assets exist: `ls build/bin/assets/audio/`

2. **No audio device available (headless environment)**
   - **Fix:** Run on system with audio device or implement null audio backend
   - Logs will show: "Failed to initialize audio device"

3. **Permissions issue**
   - **Fix:** Ensure audio files are readable
   - Check file permissions: `ls -la build/bin/assets/audio/`

4. **Audio backend initialization failure**
   - **Fix:** Check system audio libraries (PulseAudio, ALSA on Linux)
   - Install missing audio libraries

### Issue: Some Sounds Missing

**Symptoms:**
- Some audio works, others don't
- Certain sound effects fail to play

**Diagnosis:**
Check logs for specific file loading failures:
```
Failed to load <sound_name> from: <full_path>
Sound: File does not exist: <path>
```

**Common Causes:**

1. **Missing audio files**
   - **Fix:** Ensure all required WAV files exist in assets directory
   - Re-copy assets: `cp -r assets/ build/bin/assets/`

2. **Corrupted audio files**
   - **Fix:** Verify file integrity, re-export from source
   - Test file: `file build/bin/assets/audio/music/peaceful.wav`

3. **Unsupported format**
   - **Fix:** Convert to WAV format
   - Check format: `ffmpeg -i <file>`

### Issue: Audio Crashes or Hangs

**Symptoms:**
- Application crashes during audio playback
- Deadlocks during initialization

**Common Causes:**

1. **GUI thread violation**
   - MusicPlayer must be initialized on GUI thread
   - Error: "must be called on the GUI thread"

2. **Resource cleanup issues**
   - Ensure proper shutdown order
   - Call `AudioSystem::shutdown()` in destructor

## Performance Considerations

### Memory Usage

- Audio files are pre-decoded to PCM format in memory
- Typical file sizes:
  - Music tracks: ~1-5 MB per 30 second track (stereo, 48kHz)
  - Sound effects: ~50-500 KB (stereo, 48kHz)
  
### CPU Usage

- Audio processing runs on dedicated thread
- Mixing is CPU-based (no GPU acceleration)
- Keep total active sounds under 32 for best performance

### Latency

- Pre-decoding ensures low-latency playback for sound effects
- Music uses buffered streaming for low memory overhead

## Development Guidelines

### Adding New Audio Files

1. Place audio file in appropriate directory (`assets/audio/music/` or `assets/audio/voices/`)
2. Add loading code in `GameEngine::loadAudioResources()`
3. Register with AudioEventHandler if needed
4. Rebuild to copy assets to bin directory

### Debugging Audio Issues

1. **Enable verbose logging:**
   ```cpp
   qInfo() << "Audio debug info";
   ```

2. **Check initialization:**
   ```
   AudioSystem initialized successfully
   MusicPlayer initialized (miniaudio backend) channels: 4
   MiniaudioBackend: Initialized successfully
   ```

3. **Verify asset loading:**
   ```
   Loading audio resources from: /path/to/app/assets/audio/
   Loaded archer voice
   Loaded peaceful music
   ```

4. **Test audio device:**
   ```bash
   # On Linux
   aplay -l  # List audio devices
   pulseaudio --check  # Check PulseAudio status
   ```

## Testing

### Manual Testing

1. Start game and check logs for audio initialization messages
2. Select units and listen for selection sounds
3. Enter combat and verify combat music plays
4. Test victory/defeat conditions for corresponding music

### Automated Testing

Currently no automated audio tests. Future improvements:
- Unit tests for AudioSystem API
- Mock audio backend for CI/CD
- Asset validation tests

## Future Improvements

- [ ] Implement null audio backend for headless environments
- [ ] Add spatial audio support (3D positional audio)
- [ ] Implement audio asset validation in build process
- [ ] Add volume fade-in/fade-out for smoother transitions
- [ ] Support for audio resource packs/mods
- [ ] Add audio debug UI panel

## References

- [miniaudio documentation](https://miniaud.io/docs/manual/index.html)
- [Qt Multimedia](https://doc.qt.io/qt-6/qtmultimedia-index.html)
- Game audio asset specifications: See `assets/audio/README.md` (if exists)
