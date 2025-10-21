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
- ✅ Thread-safe singleton instance

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
        // Play the sound
    }
    
    void handleMusicTrigger(const MusicTriggerEvent& event) {
        // Play the music
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

## Performance Considerations

- Event dispatch is O(n) where n = number of subscribers for that event type
- Type checking happens at compile time (zero runtime cost)
- EventManager uses a singleton pattern (initialized on first use)
- No dynamic memory allocation during event dispatch
- Handlers are stored as type-erased function pointers for efficiency

## Future Enhancements

Potential improvements for the event system:

- [ ] Event queuing and deferred processing
- [ ] Priority-based event dispatch
- [ ] Event filtering/interception
- [ ] Thread-safe event publishing
- [ ] Event replay/recording for debugging
- [ ] Performance profiling hooks
