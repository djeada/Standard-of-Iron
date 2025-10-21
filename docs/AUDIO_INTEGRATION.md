# Audio Integration Documentation

## Overview

The audio subsystem has been successfully integrated into the game engine's main loop. This document describes the implementation, features, and usage of the audio system.

## Components

### AudioSystem
- **Location**: `game/audio/AudioSystem.h/cpp`
- **Purpose**: Core audio system that manages sound effects and music playback
- **Features**:
  - Thread-based audio processing to avoid blocking the game loop
  - Support for both sound effects (QSoundEffect) and music (QMediaPlayer)
  - Volume control (master, sound, music)
  - Queue-based event system for thread-safe operation

### AudioEventHandler
- **Location**: `game/audio/AudioEventHandler.h/cpp`
- **Purpose**: Bridges game events to audio playback
- **Features**:
  - Subscribes to game events (unit selection, ambient state changes, etc.)
  - Maps unit types to voice sounds
  - Maps ambient states to background music
  - Handles audio and music trigger events

## Integration Points

### 1. Initialization (GameEngine constructor)

```cpp
// Initialize AudioSystem
AudioSystem::getInstance().initialize();

// Load audio resources
loadAudioResources();

// Initialize AudioEventHandler
m_audioEventHandler = std::make_unique<Game::Audio::AudioEventHandler>(m_world.get());
m_audioEventHandler->initialize();

// Configure mappings
m_audioEventHandler->loadUnitVoiceMapping("archer", "archer_voice");
m_audioEventHandler->loadAmbientMusic(Engine::Core::AmbientState::PEACEFUL, "music_peaceful");
// ... more mappings
```

### 2. Update Loop (GameEngine::update)

The ambient state is checked periodically (every 2 seconds) to determine the appropriate background music:

```cpp
void GameEngine::updateAmbientState(float dt) {
  // Check every 2 seconds
  m_ambientCheckTimer += dt;
  if (m_ambientCheckTimer < 2.0f) return;
  
  // Determine new ambient state
  Engine::Core::AmbientState newState = Engine::Core::AmbientState::PEACEFUL;
  
  if (victory/defeat) {
    newState = VICTORY/DEFEAT;
  } else if (isPlayerInCombat()) {
    newState = COMBAT;
  } else if (both players have barracks) {
    newState = TENSE;
  }
  
  // Publish state change event if changed
  if (newState != m_currentAmbientState) {
    EventManager::instance().publish(
      AmbientStateChangedEvent(newState, previousState));
  }
}
```

### 3. Combat Detection

The `isPlayerInCombat()` method determines if the player is actively engaged:
- Checks if any player unit has an attack target
- Checks for enemies within 15 units radius of player units
- Returns true if combat is detected

### 4. Unit Selection

Unit selection events are automatically published by the SelectionSystem when units are selected. The AudioEventHandler subscribes to these events and plays the appropriate voice sound based on unit type.

### 5. Shutdown (GameEngine destructor)

```cpp
GameEngine::~GameEngine() {
  // Shutdown audio subsystem
  if (m_audioEventHandler) {
    m_audioEventHandler->shutdown();
  }
  AudioSystem::getInstance().shutdown();
}
```

## Audio Resources

Audio files are located in `assets/audio/`:

### Voices (`assets/audio/voices/`)
- `archer_voice.wav` - Played when archer is selected
- `knight_voice.wav` - Played when knight is selected
- `spearman_voice.wav` - Played when spearman is selected

### Music (`assets/audio/music/`)
- `peaceful.wav` - Peaceful ambient music (no enemies detected)
- `tense.wav` - Tense music (enemies present but not engaged)
- `combat.wav` - Combat music (active battle)
- `victory.wav` - Victory music
- `defeat.wav` - Defeat music

## Event System

The audio integration uses the existing event system:

### Published Events
1. **UnitSelectedEvent** - Published by SelectionSystem when unit is selected
2. **AmbientStateChangedEvent** - Published by GameEngine when ambient state changes
3. **AudioTriggerEvent** - Can be published to trigger specific sounds
4. **MusicTriggerEvent** - Can be published to trigger specific music

### Event Flow
```
Game Action → Event Published → AudioEventHandler Receives → AudioSystem Plays Audio
```

## Performance Considerations

1. **Thread Safety**: Audio processing runs on a separate thread to avoid blocking the game loop
2. **Update Frequency**: Ambient state is only checked every 2 seconds, not every frame
3. **Combat Detection**: Spatial checks use distance squared to avoid expensive square root calculations
4. **Event-Driven**: Audio only plays in response to events, not continuously polling

## Testing

To verify the integration:

1. **Startup**: Check logs for "AudioSystem initialized successfully" and "Audio resources loading complete"
2. **Unit Selection**: Select different unit types and verify voice sounds play
3. **Ambient Music**: 
   - Start game → peaceful music
   - Approach enemies → tense music
   - Engage in combat → combat music
   - Win/lose → victory/defeat music
4. **Shutdown**: Verify clean shutdown with no hanging threads

## Future Enhancements

Possible improvements:
- Add sound effects for combat hits, arrow launches, building capture
- Implement audio occlusion based on terrain
- Add 3D positional audio for units
- Support for dynamic music crossfading
- Audio settings in game options (volume sliders, mute)
- More unit voices and ambient tracks
