# Audio System

The audio system provides thread-safe audio playback for sounds and music using QtMultimedia, with full integration into the game's event system.

## Features

- **Singleton Pattern**: Single instance manages all audio resources
- **Thread-Safe**: Event queue with dedicated audio thread
- **Sound Effects**: Short sounds with priority and looping support
- **Music Streams**: Background music with crossfade capability
- **Volume Controls**: Master, sound, music, and voice volume controls
- **Resource Management**: Load and manage multiple audio files with dynamic loading/unloading
- **Event Integration**: Automatic audio responses to game events
- **Channel Management**: Configurable max channels with priority-based sound eviction
- **Audio Categories**: Separate volume control for SFX, Voice, and Music
- **Memory Optimization**: Dynamic resource loading and unloading for efficient memory usage

## Components

### AudioSystem
Main audio playback system that manages sound effects and music tracks.

### AudioEventHandler  
Connects game events to audio responses, enabling automatic sound/music playback based on game state.

## API Overview

### Initialization

```cpp
#include "game/audio/AudioSystem.h"
#include "game/audio/AudioEventHandler.h"

auto& audioSystem = AudioSystem::getInstance();
audioSystem.initialize();

// Create handler with world reference
Engine::Core::World world;
Game::Audio::AudioEventHandler handler(&world);
handler.initialize();
```

### Loading Audio

```cpp
// Load sound effects with categories
audioSystem.loadSound("archer_voice", "assets/audio/voices/archer_voice.wav", AudioCategory::VOICE);
audioSystem.loadSound("explosion", "assets/sounds/explosion.wav", AudioCategory::SFX);

// Load background music
audioSystem.loadMusic("peaceful", "assets/audio/music/peaceful.wav");
audioSystem.loadMusic("combat", "assets/audio/music/combat.wav");
```

### Dynamic Resource Management

```cpp
// Unload specific resources when no longer needed
audioSystem.unloadSound("explosion");
audioSystem.unloadMusic("old_track");

// Bulk unload for level transitions
audioSystem.unloadAllSounds();
audioSystem.unloadAllMusic();

// Configure channel limits
audioSystem.setMaxChannels(32);
size_t activeChannels = audioSystem.getActiveChannelCount();
```

### Configuring Event Mappings

```cpp
// Map unit types to voice lines
handler.loadUnitVoiceMapping("archer", "archer_voice");
handler.loadUnitVoiceMapping("knight", "knight_voice");

// Map ambient states to music
handler.loadAmbientMusic(Engine::Core::AmbientState::PEACEFUL, "peaceful");
handler.loadAmbientMusic(Engine::Core::AmbientState::COMBAT, "combat");
```

### Playing Audio

```cpp
// Direct playback with category
audioSystem.playSound("explosion", 1.0f, false, 10, AudioCategory::SFX);
audioSystem.playSound("unit_voice", 1.0f, false, 5, AudioCategory::VOICE);
audioSystem.playMusic("theme", 0.8f, true);

// Event-triggered playback
Engine::Core::EventManager::instance().publish(
    Engine::Core::AudioTriggerEvent("explosion", 0.8f));

Engine::Core::EventManager::instance().publish(
    Engine::Core::MusicTriggerEvent("combat", 0.6f));
```

### Volume Control

```cpp
// Set master volume (affects all audio)
audioSystem.setMasterVolume(0.5f);

// Set sound effects volume
audioSystem.setSoundVolume(0.7f);

// Set music volume
audioSystem.setMusicVolume(0.6f);

// Set voice volume (separate from SFX)
audioSystem.setVoiceVolume(0.8f);

// Get current volume levels
float master = audioSystem.getMasterVolume();
float sfx = audioSystem.getSoundVolume();
float music = audioSystem.getMusicVolume();
float voice = audioSystem.getVoiceVolume();
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
handler.shutdown();
audioSystem.shutdown();
```

## Event Integration

The audio system responds to the following events:

### UnitSelectedEvent
When a unit is selected, plays the corresponding voice line based on unit type.
- **Published by**: `SelectionSystem::selectUnit()`
- **Handler**: `AudioEventHandler::onUnitSelected()`

### AmbientStateChangedEvent  
When the ambient state changes (e.g., from peaceful to combat), switches background music.
- **Published by**: Game logic when combat state changes
- **Handler**: `AudioEventHandler::onAmbientStateChanged()`
- **States**: PEACEFUL, TENSE, COMBAT, VICTORY, DEFEAT

### AudioTriggerEvent
Direct audio playback trigger with full control over parameters.
- **Published by**: Any game system
- **Handler**: `AudioEventHandler::onAudioTrigger()`
- **Parameters**: soundId, volume, loop, priority

### MusicTriggerEvent
Direct music playback trigger with crossfade support.
- **Published by**: Any game system  
- **Handler**: `AudioEventHandler::onMusicTrigger()`
- **Parameters**: musicId, volume, crossfade

## Audio Resources

### Directory Structure
```
assets/audio/
├── voices/          # Unit voice lines
│   ├── archer_voice.wav
│   ├── knight_voice.wav
│   └── spearman_voice.wav
└── music/           # Background music
    ├── peaceful.wav
    ├── tense.wav
    ├── combat.wav
    ├── victory.wav
    └── defeat.wav
```

### Placeholder Files
Current audio files are simple sine wave tones for testing. Replace with actual game audio.

## Implementation Details

### AudioSystem
- Thread-safe event queue for audio commands
- Dedicated audio thread for non-blocking playback
- Resource management for sounds and music with dynamic loading/unloading
- Volume controls with proper mixing for Master, SFX, Music, and Voice
- Channel management with configurable limits
- Priority-based sound eviction when channel limit is reached
- Memory-safe cleanup on shutdown
- Atomic volume controls for thread safety

### Channel Management
The system implements intelligent channel management:
- Configurable maximum simultaneous sounds (default: 32 channels)
- Priority-based eviction when channel limit is reached
- Lower priority sounds are stopped to make room for higher priority sounds
- Non-looping sounds are automatically cleaned up when finished
- Active channel count tracking for monitoring

### Audio Categories
Three distinct audio categories with independent volume control:
- **SFX**: Sound effects (explosions, impacts, etc.)
- **VOICE**: Unit voices and dialogue
- **MUSIC**: Background music and themes

### AudioEventHandler
- Subscribes to game events via EventManager
- Maps unit types to voice line sounds
- Maps ambient states to background music
- Uses ScopedEventSubscription for automatic cleanup

### Sound
- Uses `QSoundEffect` for low-latency sound effects
- Supports looping and volume control
- Suitable for short audio clips

### Music
- Uses `QMediaPlayer` for streaming music
- Supports looping via signal connections
- Suitable for longer audio tracks

## Testing

The audio system can be tested through the main application by:
- Selecting units to trigger voice lines
- Observing ambient state changes to hear music transitions
- Publishing audio events through the event system

Note: Dedicated test executables are not currently part of the build configuration.

## Dependencies

- Qt5/Qt6 Multimedia module
- C++20 standard library (thread, atomic, mutex)
- Game event system (EventManager)

## Future Enhancements

- [ ] 3D positional audio
- [ ] Audio occlusion
- [ ] Dynamic music layers based on intensity
- [ ] Voice line queuing to prevent overlaps
- [ ] Audio resource hot-reloading
- [ ] Audio pooling for frequently used sounds
- [ ] Fade-in/fade-out effects for smooth transitions

