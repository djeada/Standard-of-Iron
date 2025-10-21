# Event System Architecture

## Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Game Components                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐           │
│  │ Combat   │  │ Victory  │  │  Audio   │  │   UI     │  ...      │
│  │ System   │  │ Service  │  │ Manager  │  │ Manager  │           │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘           │
│       │publish      │publish       │subscribe    │subscribe        │
│       ▼             ▼              ▼             ▼                  │
└───────┼─────────────┼──────────────┼─────────────┼──────────────────┘
        │             │              │             │
        └─────────────┴──────────────┴─────────────┘
                         │
                         ▼
        ┌────────────────────────────────────────────┐
        │         EventManager (Singleton)           │
        │  ┌──────────────────────────────────────┐ │
        │  │  Thread-Safe Event Dispatcher        │ │
        │  │  • Mutex-protected operations        │ │
        │  │  • Lock-free handler invocation      │ │
        │  │  • Statistics tracking               │ │
        │  └──────────────────────────────────────┘ │
        │                                            │
        │  Event Type Registry:                     │
        │  ┌────────────────────────────────────┐  │
        │  │ UnitSelectedEvent → [Handler₁...]  │  │
        │  │ BattleStartedEvent → [Handler₂...] │  │
        │  │ BattleEndedEvent → [Handler₃...]   │  │
        │  │ AmbientStateChanged → [Handler₄...]│  │
        │  │ AudioTriggerEvent → [Handler₅...]  │  │
        │  │ MusicTriggerEvent → [Handler₆...]  │  │
        │  └────────────────────────────────────┘  │
        └────────────────────────────────────────────┘
```

## Event Flow

### Publishing an Event

```
┌──────────────┐
│ Game Logic   │
└──────┬───────┘
       │ 1. Create event
       │    BattleStartedEvent(attacker, defender)
       ▼
┌──────────────┐
│ publish()    │
└──────┬───────┘
       │ 2. Lock mutex
       │ 3. Find subscribers
       │ 4. Copy handler list
       │ 5. Unlock mutex
       │ 6. Update stats
       ▼
┌──────────────┐
│ Invoke       │  7. Call each handler
│ Handlers     │     (no lock held)
└──────┬───────┘
       │ 8. Handler processes event
       ▼
┌──────────────┐
│ Handlers     │  AudioManager: play sound
│ Execute      │  UIManager: show notification
└──────────────┘  VFXManager: spawn particles
```

### Subscribing to Events

```
┌──────────────┐
│ Component    │
│ Constructor  │
└──────┬───────┘
       │ 1. Create subscription
       │    subscribe<BattleStartedEvent>(handler)
       ▼
┌──────────────┐
│ subscribe()  │  2. Lock mutex
└──────┬───────┘  3. Generate unique handle
       │          4. Wrap handler
       │          5. Store in registry
       │          6. Update stats
       │          7. Unlock mutex
       ▼
┌──────────────┐
│ Return       │  8. Return handle
│ Handle       │     (or ScopedEventSubscription)
└──────────────┘
```

## Event Types Hierarchy

```
                    ┌──────────┐
                    │  Event   │ (Base class)
                    └────┬─────┘
                         │
        ┌────────────────┴────────────────┐
        │                                 │
   ┌────▼─────┐                    ┌─────▼────┐
   │  Game    │                    │  System  │
   │  Events  │                    │  Events  │
   └────┬─────┘                    └────┬─────┘
        │                               │
   ┌────┴──────────────────┐      ┌────┴──────┐
   │                       │      │            │
┌──▼───────────┐  ┌───────▼─────┐  ┌────▼────┐
│ Unit Events  │  │Battle Events│  │  Audio  │
├──────────────┤  ├─────────────┤  │  Events │
│ • Selected   │  │ • Started   │  ├─────────┤
│ • Moved      │  │ • Ended     │  │ • Trigger│
│ • Died       │  └─────────────┘  │ • Music  │
│ • Spawned    │                   └─────────┘
└──────────────┘
       │
┌──────▼─────────┐
│Building Events │
├────────────────┤
│ • Attacked     │
│ • Captured     │
└────────────────┘
```

## Key Design Patterns

### 1. Singleton Pattern
```cpp
EventManager::instance() // Always returns same instance
```
**Why?** Centralized event management, global access

### 2. Observer Pattern
```cpp
subscribe() → Observer registers interest
publish()   → Notify all observers
```
**Why?** Decoupled communication between components

### 3. RAII Pattern
```cpp
ScopedEventSubscription<T> // Automatic cleanup
```
**Why?** Prevent memory leaks and dangling handlers

### 4. Template Metaprogramming
```cpp
template<typename T>
void publish(const T& event) // Compile-time type safety
```
**Why?** Type safety without runtime overhead

## Thread Safety Mechanisms

### Mutex Protection
```
┌────────────────────────────────────┐
│ Every public method:               │
│                                    │
│ subscribe()   ──→ lock_guard       │
│ unsubscribe() ──→ lock_guard       │
│ publish()     ──→ lock_guard       │
│                   (during copy)    │
└────────────────────────────────────┘
```

### Lock-Free Dispatch
```
publish() flow:
1. Lock mutex
2. Copy handlers vector
3. Unlock mutex
4. Iterate and invoke handlers (no lock!)
```

**Why?** Prevents deadlock if handler calls subscribe/unsubscribe

## Statistics Tracking

```cpp
struct EventStats {
    size_t publishCount;     // Total times published
    size_t subscriberCount;  // Current subscribers
};

// Per event type
map<type_index, EventStats> m_stats;
```

**Use Cases:**
- Debug: "Why isn't my event being handled?"
- Performance: "How many times is this event fired?"
- Validation: "Are all expected listeners registered?"

## Performance Characteristics

| Operation      | Complexity | Notes                              |
|----------------|------------|------------------------------------|
| subscribe()    | O(1)       | Append to vector                   |
| unsubscribe()  | O(n)       | Linear search + erase              |
| publish()      | O(n)       | Call each of n subscribers         |
| getStats()     | O(1)       | Map lookup                         |

Where n = number of subscribers for that event type

## Memory Layout

```
EventManager
├── mutex m_mutex                          // Synchronization
├── map<type_index, vector<HandlerEntry>>  // Handler storage
│   ├── [UnitSelectedEvent]
│   │   └── [Handler₁, Handler₂, ...]
│   ├── [BattleStartedEvent]
│   │   └── [Handler₁, Handler₂, ...]
│   └── ...
├── map<type_index, EventStats>            // Statistics
│   ├── [UnitSelectedEvent] → {pub:10, sub:2}
│   └── ...
└── size_t m_nextHandle                    // Handle counter
```

## Integration Points

### With Audio System
```
Game Event → EventManager → AudioManager → AudioSystem
                   ↓
            AudioTriggerEvent
            MusicTriggerEvent
```

### With Combat System
```
Combat Logic → BattleStartedEvent → EventManager
                                          ↓
                                    [Listeners]
                                    • AudioManager
                                    • VFXManager
                                    • UIManager
                                    • StatisticsTracker
```

### With UI System
```
User Input → UnitSelectedEvent → EventManager
                                        ↓
                                  [Listeners]
                                  • UIManager (highlight)
                                  • AudioManager (sound)
                                  • CameraSystem (focus)
```

## Scalability Features

1. **Type Isolation**: Each event type has independent subscriber list
2. **Thread Safety**: Can publish/subscribe from multiple threads
3. **Statistics**: Monitor system health and usage
4. **RAII**: Automatic cleanup prevents leaks
5. **Lock-Free Dispatch**: Handlers can't deadlock the system
6. **Named Events**: Easy debugging with getTypeName()

## Future Enhancements

### Event Queuing
```
publish() → Queue → ProcessEventsLater()
```
**Benefit**: Decouple event processing from game logic timing

### Priority System
```
subscribe(handler, priority=HIGH)
```
**Benefit**: Control handler execution order

### Event Filtering
```
subscribe(handler, filter=[owner==PLAYER])
```
**Benefit**: Reduce unnecessary handler invocations

### Event Recording
```
EventRecorder → Playback for debugging/testing
```
**Benefit**: Reproduce bugs, unit testing scenarios

## Best Practices

1. ✅ **Use ScopedEventSubscription** in classes
   - Automatic cleanup on destruction
   - No manual unsubscribe needed

2. ✅ **Keep handlers lightweight**
   - Defer heavy work to next frame
   - Avoid blocking operations

3. ✅ **Don't publish in handlers**
   - Can cause recursion
   - Use deferred publishing if needed

4. ✅ **Use const references**
   - `const T& event` in handlers
   - Prevents accidental modification

5. ✅ **Document custom events**
   - When fired, what data they contain
   - Expected handlers

## Testing Strategy

```
test_events.cpp covers:
├── Basic functionality
│   ├── Subscribe/Publish
│   ├── Multiple subscribers
│   └── Unsubscribe
├── RAII behavior
│   └── ScopedEventSubscription
├── Event types
│   ├── Unit events
│   ├── Battle events
│   ├── Ambient events
│   └── Audio events
├── Edge cases
│   ├── No subscribers
│   └── Data integrity
└── System features
    ├── Statistics
    └── Type names
```

## Conclusion

The event system provides:
- ✅ Decoupled component communication
- ✅ Thread-safe operation
- ✅ Type-safe compile-time checking
- ✅ Easy to test and maintain
- ✅ Scalable architecture
- ✅ Modern C++20 design
- ✅ Comprehensive documentation

Ready for production use in Standard of Iron game engine!
