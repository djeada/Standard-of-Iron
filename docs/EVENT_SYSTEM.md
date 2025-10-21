# Event System Documentation

## Overview

The Event System provides a flexible publish-subscribe mechanism for communication between game components. It allows decoupled components to communicate without direct dependencies.

## Architecture

The event system is implemented in `game/core/event_manager.h` and consists of:

- **Event Base Class**: All events inherit from `Engine::Core::Event`
- **EventManager**: Singleton managing event subscriptions and publications
- **EventHandler**: Type-safe callback function for event handling
- **ScopedEventSubscription**: RAII wrapper for automatic unsubscription

## Key Features

- ✅ Type-safe event handling using templates
- ✅ Multiple subscribers per event type
- ✅ Manual and automatic (RAII) subscription management
- ✅ Zero-cost abstractions with compile-time type checking
- ✅ Thread-safe event publishing and subscription
- ✅ Event statistics tracking (publish counts, subscriber counts)
- ✅ Named event types for debugging
- ✅ Lock-free event dispatch (copies handlers before calling)

## Event Types

### Unit Events

#### UnitSelectedEvent
Triggered when a unit is selected by the player.

```cpp
EventManager::instance().publish(UnitSelectedEvent(unitId));
```

Fields:
- `EntityID unitId`: The selected unit's ID

#### UnitMovedEvent
Triggered when a unit moves to a new position.

```cpp
EventManager::instance().publish(UnitMovedEvent(unitId, x, y));
```

Fields:
- `EntityID unitId`: The moving unit's ID
- `float x, y`: New position coordinates

#### UnitDiedEvent
Triggered when a unit is destroyed.

```cpp
EventManager::instance().publish(
    UnitDiedEvent(unitId, ownerId, unitType, killerId, killerOwnerId)
);
```

Fields:
- `EntityID unitId`: The dying unit's ID
- `int ownerId`: Owner of the dying unit
- `std::string unitType`: Type of unit (e.g., "knight", "archer")
- `EntityID killerId`: Optional killer unit ID
- `int killerOwnerId`: Optional killer owner ID

#### UnitSpawnedEvent
Triggered when a new unit is created.

```cpp
EventManager::instance().publish(UnitSpawnedEvent(unitId, ownerId, unitType));
```

Fields:
- `EntityID unitId`: The new unit's ID
- `int ownerId`: Owner of the unit
- `std::string unitType`: Type of unit

### Building Events

#### BuildingAttackedEvent
Triggered when a building takes damage.

```cpp
EventManager::instance().publish(
    BuildingAttackedEvent(buildingId, ownerId, buildingType, 
                         attackerId, attackerOwnerId, damage)
);
```

Fields:
- `EntityID buildingId`: The building's ID
- `int ownerId`: Building owner
- `std::string buildingType`: Type of building
- `EntityID attackerId`: Attacking unit ID
- `int attackerOwnerId`: Attacker owner
- `int damage`: Damage amount

#### BarrackCapturedEvent
Triggered when a barracks changes ownership.

```cpp
EventManager::instance().publish(
    BarrackCapturedEvent(barrackId, previousOwnerId, newOwnerId)
);
```

Fields:
- `EntityID barrackId`: The barracks ID
- `int previousOwnerId`: Previous owner
- `int newOwnerId`: New owner

### Battle Events

#### BattleStartedEvent
Triggered when combat begins between two units.

```cpp
EventManager::instance().publish(
    BattleStartedEvent(attackerId, defenderId, posX, posY)
);
```

Fields:
- `EntityID attackerId`: Attacking unit ID
- `EntityID defenderId`: Defending unit ID
- `float posX, posY`: Battle location

#### BattleEndedEvent
Triggered when combat concludes.

```cpp
EventManager::instance().publish(
    BattleEndedEvent(winnerId, loserId, defenderDied)
);
```

Fields:
- `EntityID winnerId`: Victorious unit ID
- `EntityID loserId`: Defeated unit ID
- `bool defenderDied`: Whether the defender was killed

### Ambient State Events

#### AmbientStateChangedEvent
Triggered when the game's ambient state changes (affects music, atmosphere).

```cpp
EventManager::instance().publish(
    AmbientStateChangedEvent(AmbientState::COMBAT, AmbientState::PEACEFUL)
);
```

Fields:
- `AmbientState newState`: New ambient state
- `AmbientState previousState`: Previous state

Ambient States:
- `PEACEFUL`: No combat, exploration
- `TENSE`: Enemy nearby, heightened awareness
- `COMBAT`: Active battle
- `VICTORY`: Player won
- `DEFEAT`: Player lost

### Audio Events

#### AudioTriggerEvent
Triggers a sound effect to play.

```cpp
EventManager::instance().publish(
    AudioTriggerEvent("sword_clash", 0.8f, false, 5)
);
```

Fields:
- `std::string soundId`: Sound identifier
- `float volume`: Volume (0.0 to 1.0)
- `bool loop`: Whether to loop
- `int priority`: Playback priority

#### MusicTriggerEvent
Triggers background music to play.

```cpp
EventManager::instance().publish(
    MusicTriggerEvent("battle_theme", 0.7f, true)
);
```

Fields:
- `std::string musicId`: Music track identifier
- `float volume`: Volume (0.0 to 1.0)
- `bool crossfade`: Whether to crossfade

## Usage Examples

### Basic Subscription

```cpp
#include "game/core/event_manager.h"

using namespace Engine::Core;

// Subscribe to an event
auto handle = EventManager::instance().subscribe<UnitSelectedEvent>(
    [](const UnitSelectedEvent& event) {
        std::cout << "Unit " << event.unitId << " selected!" << std::endl;
    }
);

// Publish an event
EventManager::instance().publish(UnitSelectedEvent(42));

// Unsubscribe when done
EventManager::instance().unsubscribe<UnitSelectedEvent>(handle);
```

### Scoped Subscription (RAII)

```cpp
{
    // Subscription automatically cleaned up when scope ends
    ScopedEventSubscription<BattleStartedEvent> subscription(
        [](const BattleStartedEvent& event) {
            std::cout << "Battle started!" << std::endl;
        }
    );
    
    // Event will be received
    EventManager::instance().publish(
        BattleStartedEvent(1, 2, 10.0f, 15.0f)
    );
    
} // Automatically unsubscribed here

// Event will not be received
EventManager::instance().publish(BattleStartedEvent(3, 4, 20.0f, 25.0f));
```

### Multiple Subscribers

```cpp
// First subscriber
auto handle1 = EventManager::instance().subscribe<AmbientStateChangedEvent>(
    [](const AmbientStateChangedEvent& event) {
        std::cout << "Subscriber 1: State changed" << std::endl;
    }
);

// Second subscriber
auto handle2 = EventManager::instance().subscribe<AmbientStateChangedEvent>(
    [](const AmbientStateChangedEvent& event) {
        std::cout << "Subscriber 2: State changed" << std::endl;
    }
);

// Both subscribers will be notified
EventManager::instance().publish(
    AmbientStateChangedEvent(AmbientState::COMBAT, AmbientState::PEACEFUL)
);
```

### Class Member Function as Handler

```cpp
class AudioController {
private:
    ScopedEventSubscription<AudioTriggerEvent> audioSubscription;
    ScopedEventSubscription<MusicTriggerEvent> musicSubscription;
    
public:
    AudioController() {
        audioSubscription = ScopedEventSubscription<AudioTriggerEvent>(
            [this](const AudioTriggerEvent& event) {
                this->handleAudioTrigger(event);
            }
        );
        
        musicSubscription = ScopedEventSubscription<MusicTriggerEvent>(
            [this](const MusicTriggerEvent& event) {
                this->handleMusicTrigger(event);
            }
        );
    }
    
    void handleAudioTrigger(const AudioTriggerEvent& event) {
        // Play the sound using audio system
        AudioSystem::getInstance().playSound(
            event.soundId, event.volume, event.loop, event.priority
        );
    }
    
    void handleMusicTrigger(const MusicTriggerEvent& event) {
        // Play the music using audio system
        AudioSystem::getInstance().playMusic(
            event.musicId, event.volume, event.crossfade
        );
    }
};
```

### Integrating with Audio System

The event system can trigger audio through dedicated audio events:

```cpp
// In combat system, when battle starts
void CombatSystem::startBattle(EntityID attacker, EntityID defender) {
    // Publish battle event
    EventManager::instance().publish(
        BattleStartedEvent(attacker, defender, posX, posY)
    );
    
    // Trigger battle sound effect
    EventManager::instance().publish(
        AudioTriggerEvent("sword_clash", 0.8f, false, 5)
    );
}

// In victory service, when player wins
void VictoryService::handleVictory() {
    // Change ambient state
    EventManager::instance().publish(
        AmbientStateChangedEvent(AmbientState::VICTORY, previousState)
    );
    
    // Play victory music
    EventManager::instance().publish(
        MusicTriggerEvent("victory_theme", 1.0f, true)
    );
}
```

### Responding to Game State Changes

Listen to game events and respond with audio:

```cpp
class GameAudioManager {
private:
    ScopedEventSubscription<AmbientStateChangedEvent> ambientSub;
    ScopedEventSubscription<BattleStartedEvent> battleSub;
    ScopedEventSubscription<UnitDiedEvent> unitDiedSub;
    
public:
    GameAudioManager() {
        // Change music based on ambient state
        ambientSub = ScopedEventSubscription<AmbientStateChangedEvent>(
            [](const AmbientStateChangedEvent& event) {
                switch (event.newState) {
                    case AmbientState::PEACEFUL:
                        EventManager::instance().publish(
                            MusicTriggerEvent("peaceful_theme", 0.6f)
                        );
                        break;
                    case AmbientState::COMBAT:
                        EventManager::instance().publish(
                            MusicTriggerEvent("battle_theme", 0.9f)
                        );
                        break;
                    // ... other states
                }
            }
        );
        
        // Play battle sounds
        battleSub = ScopedEventSubscription<BattleStartedEvent>(
            [](const BattleStartedEvent& event) {
                EventManager::instance().publish(
                    AudioTriggerEvent("battle_start", 0.7f)
                );
            }
        );
        
        // Play death sounds
        unitDiedSub = ScopedEventSubscription<UnitDiedEvent>(
            [](const UnitDiedEvent& event) {
                std::string deathSound = event.unitType + "_death";
                EventManager::instance().publish(
                    AudioTriggerEvent(deathSound, 0.8f)
                );
            }
        );
    }
};
```

## Testing

A standalone test application verifies the event system functionality:

```bash
make build
./build/test_events
```

The test suite covers:
- Basic publish/subscribe
- Multiple subscribers
- Unsubscription
- Scoped subscriptions (RAII)
- All event types
- Data integrity
- Edge cases

## Best Practices

1. **Use ScopedEventSubscription for object lifetimes**: Prevents memory leaks and dangling handlers
2. **Keep event handlers lightweight**: Heavy processing should be deferred
3. **Avoid publishing events from within handlers**: Can cause recursion or ordering issues
4. **Use descriptive event data**: Include all context needed by subscribers
5. **Document custom events**: Help other developers understand when and why they're triggered

## Creating Custom Events

To add a new event type:

1. Define event class inheriting from `Event`:

```cpp
class MyCustomEvent : public Event {
public:
    MyCustomEvent(int value) : value(value) {}
    int value;
};
```

2. Add to `event_manager.h` in the appropriate section

3. Publish and subscribe as needed:

```cpp
EventManager::instance().subscribe<MyCustomEvent>([](const MyCustomEvent& e) {
    // Handle event
});

EventManager::instance().publish(MyCustomEvent(123));
```

## Thread Safety

The event system is fully thread-safe:

- All operations (subscribe, unsubscribe, publish) are protected by mutexes
- Event dispatch makes a copy of handlers before calling them
- This prevents deadlocks when handlers modify subscriptions
- Multiple threads can safely publish and subscribe concurrently

## Event Statistics

The EventManager tracks statistics for each event type:

```cpp
#include <typeindex>

// Get statistics for an event type
auto stats = EventManager::instance().getStats(typeid(UnitDiedEvent));
std::cout << "Published: " << stats.publishCount << std::endl;
std::cout << "Subscribers: " << stats.subscriberCount << std::endl;

// Get current subscriber count
size_t count = EventManager::instance().getSubscriberCount(typeid(BattleStartedEvent));
```

## Performance Considerations

- Event dispatch is O(n) where n = number of subscribers for that event type
- Type checking happens at compile time (zero runtime cost)
- EventManager uses a singleton pattern (initialized on first use)
- Handler copying during dispatch adds minimal overhead but prevents deadlocks
- Handlers are stored as type-erased function pointers for efficiency
- Mutex overhead is minimal for single-threaded use
- Statistics tracking adds negligible overhead

## Future Enhancements

Potential improvements for the event system:

- [ ] Event queuing and deferred processing
- [ ] Priority-based event dispatch
- [ ] Event filtering/interception
- [ ] Event replay/recording for debugging
- [ ] Performance profiling hooks
- [ ] Weak pointer support for automatic cleanup
