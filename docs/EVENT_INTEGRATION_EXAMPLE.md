# Event System Integration Example

This document shows a practical example of integrating the event system with game components.

## Audio Integration Example

Here's how to create a game audio manager that responds to game events:

### GameAudioManager.h

```cpp
#pragma once

#include "game/audio/AudioSystem.h"
#include "game/core/event_manager.h"

class GameAudioManager {
public:
  GameAudioManager();
  ~GameAudioManager() = default;

  void initialize();
  void shutdown();

private:
  void handleUnitSelected(const Engine::Core::UnitSelectedEvent &event);
  void handleBattleStarted(const Engine::Core::BattleStartedEvent &event);
  void handleBattleEnded(const Engine::Core::BattleEndedEvent &event);
  void handleAmbientStateChanged(
      const Engine::Core::AmbientStateChangedEvent &event);
  void handleUnitDied(const Engine::Core::UnitDiedEvent &event);
  void handleUnitSpawned(const Engine::Core::UnitSpawnedEvent &event);

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>
      m_unitSelectedSub;
  Engine::Core::ScopedEventSubscription<Engine::Core::BattleStartedEvent>
      m_battleStartedSub;
  Engine::Core::ScopedEventSubscription<Engine::Core::BattleEndedEvent>
      m_battleEndedSub;
  Engine::Core::ScopedEventSubscription<
      Engine::Core::AmbientStateChangedEvent>
      m_ambientStateSub;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSub;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unitSpawnedSub;

  Engine::Core::AmbientState m_currentAmbientState =
      Engine::Core::AmbientState::PEACEFUL;
};
```

### GameAudioManager.cpp

```cpp
#include "GameAudioManager.h"
#include <iostream>

GameAudioManager::GameAudioManager() {}

void GameAudioManager::initialize() {
  std::cout << "Initializing GameAudioManager..." << std::endl;

  // Subscribe to game events
  m_unitSelectedSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>(
          [this](const Engine::Core::UnitSelectedEvent &event) {
            this->handleUnitSelected(event);
          });

  m_battleStartedSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::BattleStartedEvent>(
          [this](const Engine::Core::BattleStartedEvent &event) {
            this->handleBattleStarted(event);
          });

  m_battleEndedSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::BattleEndedEvent>(
          [this](const Engine::Core::BattleEndedEvent &event) {
            this->handleBattleEnded(event);
          });

  m_ambientStateSub = Engine::Core::ScopedEventSubscription<
      Engine::Core::AmbientStateChangedEvent>(
      [this](const Engine::Core::AmbientStateChangedEvent &event) {
        this->handleAmbientStateChanged(event);
      });

  m_unitDiedSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &event) {
            this->handleUnitDied(event);
          });

  m_unitSpawnedSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent &event) {
            this->handleUnitSpawned(event);
          });

  std::cout << "GameAudioManager initialized and listening for events."
            << std::endl;
}

void GameAudioManager::shutdown() {
  std::cout << "Shutting down GameAudioManager..." << std::endl;
  // Subscriptions automatically cleaned up by ScopedEventSubscription
}

void GameAudioManager::handleUnitSelected(
    const Engine::Core::UnitSelectedEvent &event) {
  // Play selection sound
  AudioSystem::getInstance().playSound("unit_select", 0.5f);
}

void GameAudioManager::handleBattleStarted(
    const Engine::Core::BattleStartedEvent &event) {
  // Play battle start sound
  AudioSystem::getInstance().playSound("sword_clash", 0.8f, false, 5);

  // If not already in combat music, change ambient state
  if (m_currentAmbientState != Engine::Core::AmbientState::COMBAT) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(
            Engine::Core::AmbientState::COMBAT, m_currentAmbientState));
  }
}

void GameAudioManager::handleBattleEnded(
    const Engine::Core::BattleEndedEvent &event) {
  if (event.defenderDied) {
    // Play death sound
    AudioSystem::getInstance().playSound("unit_death", 0.7f);
  } else {
    // Play retreat sound
    AudioSystem::getInstance().playSound("unit_retreat", 0.6f);
  }
}

void GameAudioManager::handleAmbientStateChanged(
    const Engine::Core::AmbientStateChangedEvent &event) {
  m_currentAmbientState = event.newState;

  // Change background music based on state
  switch (event.newState) {
  case Engine::Core::AmbientState::PEACEFUL:
    AudioSystem::getInstance().playMusic("peaceful_theme", 0.4f, true);
    break;

  case Engine::Core::AmbientState::TENSE:
    AudioSystem::getInstance().playMusic("tense_theme", 0.6f, true);
    break;

  case Engine::Core::AmbientState::COMBAT:
    AudioSystem::getInstance().playMusic("battle_theme", 0.8f, true);
    break;

  case Engine::Core::AmbientState::VICTORY:
    AudioSystem::getInstance().playMusic("victory_theme", 0.9f, false);
    AudioSystem::getInstance().playSound("victory_fanfare", 1.0f);
    break;

  case Engine::Core::AmbientState::DEFEAT:
    AudioSystem::getInstance().playMusic("defeat_theme", 0.7f, false);
    break;
  }
}

void GameAudioManager::handleUnitDied(
    const Engine::Core::UnitDiedEvent &event) {
  // Play unit-specific death sound
  std::string deathSound = event.unitType + "_death";
  AudioSystem::getInstance().playSound(deathSound, 0.75f);
}

void GameAudioManager::handleUnitSpawned(
    const Engine::Core::UnitSpawnedEvent &event) {
  // Play spawn sound
  std::string spawnSound = event.unitType + "_spawn";
  AudioSystem::getInstance().playSound(spawnSound, 0.6f);
}
```

## Usage in Main Game Engine

### In GameEngine.h

```cpp
class GameEngine {
private:
    std::unique_ptr<GameAudioManager> m_audioManager;
    // ... other members
};
```

### In GameEngine.cpp

```cpp
void GameEngine::initialize() {
    // Initialize audio system
    AudioSystem::getInstance().initialize();
    
    // Load audio resources
    AudioSystem::getInstance().loadSound("unit_select", "assets/audio/ui_select.wav");
    AudioSystem::getInstance().loadSound("sword_clash", "assets/audio/battle_start.wav");
    // ... load other sounds
    
    // Create and initialize audio manager
    m_audioManager = std::make_unique<GameAudioManager>();
    m_audioManager->initialize();
    
    // Set initial ambient state
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(
            Engine::Core::AmbientState::PEACEFUL,
            Engine::Core::AmbientState::PEACEFUL
        )
    );
    
    // ... rest of initialization
}

void GameEngine::shutdown() {
    // Shutdown in reverse order
    m_audioManager->shutdown();
    m_audioManager.reset();
    
    AudioSystem::getInstance().shutdown();
    
    // ... rest of shutdown
}
```

## Benefits of This Approach

1. **Decoupling**: Game logic doesn't need to know about audio system
2. **Centralized**: All audio logic in one place
3. **Easy to Test**: Can test game logic without audio
4. **Easy to Modify**: Change audio behavior without touching game code
5. **Thread-Safe**: Event system handles synchronization
6. **Automatic Cleanup**: ScopedEventSubscription handles unsubscription

## Extending the System

### Adding Visual Effects

Similarly, you could create a `GameVFXManager`:

```cpp
class GameVFXManager {
private:
    Engine::Core::ScopedEventSubscription<Engine::Core::BattleStartedEvent> m_battleSub;
    
public:
    void initialize() {
        m_battleSub = Engine::Core::ScopedEventSubscription<Engine::Core::BattleStartedEvent>(
            [this](const Engine::Core::BattleStartedEvent& event) {
                // Spawn particle effects at battle location
                spawnBattleParticles(event.posX, event.posY);
            }
        );
    }
    
    void spawnBattleParticles(float x, float y) {
        // Create visual effect
    }
};
```

### Adding UI Updates

Create a `GameUIManager`:

```cpp
class GameUIManager {
private:
    Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent> m_unitSelectSub;
    
public:
    void initialize() {
        m_unitSelectSub = Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>(
            [this](const Engine::Core::UnitSelectedEvent& event) {
                // Update UI to show selected unit info
                updateSelectedUnitDisplay(event.unitId);
            }
        );
    }
    
    void updateSelectedUnitDisplay(EntityID unitId) {
        // Update UI elements
    }
};
```

## Testing the Integration

You can test each manager independently:

```cpp
void testAudioManager() {
    GameAudioManager manager;
    manager.initialize();
    
    // Trigger events
    Engine::Core::EventManager::instance().publish(
        Engine::Core::BattleStartedEvent(1, 2, 10.0f, 20.0f)
    );
    
    // Verify audio was triggered (would need mocking for full test)
    
    manager.shutdown();
}
```

## Performance Considerations

- Each manager adds one subscriber per event type
- Event dispatch is O(n) where n = total subscribers
- Keep handlers lightweight - defer heavy work
- Audio system queues commands internally (async processing)
- No blocking operations in event handlers

## Best Practices

1. **Initialize early**: Set up managers before game starts
2. **Clean shutdown**: Shutdown managers before audio system
3. **Load resources first**: Load audio before triggering events
4. **Handle missing resources**: Check if sounds exist before playing
5. **Use appropriate volumes**: Different sounds should have different volumes
6. **Priority system**: Combat sounds should have higher priority
7. **Limit simultaneous sounds**: Prevent audio overload
8. **State tracking**: Track current state to avoid redundant transitions
