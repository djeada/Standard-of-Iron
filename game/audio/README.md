# Audio System

The audio system provides thread-safe audio playback for sounds and music using QtMultimedia.

## Features

- **Singleton Pattern**: Single instance manages all audio resources
- **Thread-Safe**: Event queue with dedicated audio thread
- **Sound Effects**: Short sounds with priority and looping support
- **Music Streams**: Background music with crossfade capability
- **Volume Controls**: Master, sound, and music volume controls
- **Resource Management**: Load and manage multiple audio files

## API Overview

### Initialization

```cpp
#include "game/audio/AudioSystem.h"

auto& audioSystem = AudioSystem::getInstance();
audioSystem.initialize();
```

### Loading Audio

```cpp
// Load a sound effect
audioSystem.loadSound("explosion", "assets/sounds/explosion.wav");

// Load background music
audioSystem.loadMusic("theme", "assets/music/theme.mp3");
```

### Playing Audio

```cpp
// Play a sound effect
audioSystem.playSound("explosion", 1.0f, false, 10);

// Play music with looping
audioSystem.playMusic("theme", 0.8f, true);
```

### Volume Control

```cpp
// Set master volume (affects all audio)
audioSystem.setMasterVolume(0.5f);

// Set sound effects volume
audioSystem.setSoundVolume(0.7f);

// Set music volume
audioSystem.setMusicVolume(0.6f);
```

### Stopping Audio

```cpp
// Stop a specific sound
audioSystem.stopSound("explosion");

// Stop all music
audioSystem.stopMusic();
```

### Pause/Resume

```cpp
// Pause all music
audioSystem.pauseAll();

// Resume all music
audioSystem.resumeAll();
```

### Shutdown

```cpp
// Clean shutdown
audioSystem.shutdown();
```

## Implementation Details

### AudioSystem
- Thread-safe event queue for audio commands
- Dedicated audio thread for non-blocking playback
- Resource management for sounds and music
- Volume controls with proper mixing

### Sound
- Uses `QSoundEffect` for low-latency sound effects
- Supports looping and volume control
- Suitable for short audio clips

### Music
- Uses `QMediaPlayer` for streaming music
- Supports looping via signal connections
- Suitable for longer audio tracks

## Testing

A standalone test application is provided in `test_audio.cpp`:

```bash
cd build
make audio_system
./test_audio
```

## Dependencies

- Qt5/Qt6 Multimedia module
- C++20 standard library (thread, atomic, mutex)

## Future Enhancements

- Audio pooling for frequently used sounds
- 3D positional audio support
- Audio mixing and effects
- Compression and streaming optimization
- Integration with game event system
