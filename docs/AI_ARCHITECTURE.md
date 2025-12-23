# RTS AI System Architecture

## Overview

The RTS AI system is designed for scalability, performance, and strategic behavior in large-scale battles. It uses a multi-threaded, behavior-based architecture with centralized decision-making and **configurable AI strategies** that allow different AI personalities and behaviors.

## Core Design Principles

1. **Centralized Decision Logic**: All AI decisions are made in a central system, not per-unit
2. **Throttled Updates**: AI evaluates at fixed intervals (default 300ms), not every frame
3. **Asynchronous Processing**: AI reasoning runs on background threads to avoid blocking the main game loop
4. **Behavior-Based Actions**: Modular behaviors with priority system allow concurrent execution
5. **Deadlock Prevention**: Built-in timeout and recovery mechanisms prevent stuck states
6. **Strategy-Based Behavior**: Configurable AI strategies modify decision-making parameters
7. **Snake Case Naming**: All code follows snake_case conventions for consistency

## Architecture Components

### 1. AISystem (Main Coordinator)

**Location**: `game/systems/ai_system.cpp`

The main system that manages all AI players and coordinates the AI pipeline.

**Key Responsibilities**:
- Initialize AI instances for each AI-controlled player
- Manage update timing and throttling
- Submit jobs to AI workers
- Process results and apply commands
- Handle event subscriptions (e.g., building attacked)

**Performance Features**:
- Configurable update interval via `set_update_interval()`
- Per-AI-instance timing to prevent synchronized spikes
- Worker busy checking to prevent job queue overflow

### 2. AISnapshot & AIContext

**Location**: `game/systems/ai_system/ai_types.h`

Core data structures for AI reasoning.

**AISnapshot** - Immutable world state snapshot
**AIContext** - Persistent AI state with deadlock tracking

### 3. State Machine

Strategic phases: Idle, Gathering, Attacking, Defending, Retreating, Expanding

Deadlock recovery triggers after 60s in same state OR 10 cycles with no progress.

### 4. Behaviors

Priority-based modular behaviors:
- **ProductionBehavior** (High, Concurrent): Unit production
- **DefendBehavior** (Critical, Exclusive): Base protection
- **AttackBehavior** (Normal, Exclusive): Offensive operations
- **GatherBehavior** (Low, Exclusive): Unit consolidation
- **RetreatBehavior** (Critical, Exclusive): Tactical withdrawal

### 5. AI Strategies

**Location**: `game/systems/ai_system/ai_strategy.h`

AI strategies allow different behaviors and personalities for AI opponents:

**Available Strategies**:
- **Balanced** (Default): Standard balanced approach for skirmish mode
- **Aggressive**: Early aggression, attacks with fewer units, lower retreat threshold
- **Defensive**: Focuses on base defense, builds larger armies before attacking
- **Expansionist**: Prioritizes capturing neutral buildings
- **Economic**: Focuses on production, builds massive armies before engaging
- **Harasser**: Hit-and-run tactics with small groups, retreats often
- **Rusher**: Ultra-aggressive early game with minimal units

**Strategy Configuration**:
Each strategy has modifiers that affect AI behavior:
- `aggression_modifier`: Affects attack timing and frequency
- `defense_modifier`: Affects defensive stance and retreat thresholds
- `expansion_priority`: Priority for capturing neutral buildings
- `production_rate_modifier`: Speed of unit production
- `min_attack_force`: Minimum force required before attacking
- `retreat_threshold`: Health percentage when AI retreats

**Personality System**:
Strategies are further customized by personality values (0.0-1.0):
- `aggression`: Additional aggression factor
- `defense`: Additional defensive stance
- `harassment`: Enables harassing behavior at range

## Performance Characteristics

- **Update Frequency**: 300ms default (3.33 Hz per AI)
- **Thread Usage**: 1 thread per AI player
- **CPU Usage**: Linear with unit count, independent of map size
- **Scalability**: Handles 500+ units per player efficiently

## Anti-Deadlock Mechanisms

1. **Time-Based**: Force transition after 60s in same state
2. **Progress-Based**: Detect 10 consecutive cycles with no action
3. **Automatic Recovery**: Clear assignments and transition to fallback state
4. **Retreat Timeout**: Force end after 12s to prevent infinite retreat

## Configuration

### Update Interval

Update interval can be adjusted:
```cpp
ai_system.set_update_interval(0.5f); // Reduce to 2 Hz for performance
```

### AI Strategy Configuration

Set AI strategy for a specific player:
```cpp
// Set aggressive strategy with high aggression personality
ai_system.set_ai_strategy(player_id, AIStrategy::Aggressive, 
                         0.9f, // aggression
                         0.3f, // defense
                         0.2f); // harassment

// Set defensive strategy
ai_system.set_ai_strategy(player_id, AIStrategy::Defensive,
                         0.4f, 0.9f, 0.2f);
```

### Mission Configuration

In mission JSON files, specify strategy for AI opponents:
```json
{
  "ai_setups": [
    {
      "id": "aggressive_enemy",
      "nation": "roman_republic",
      "strategy": "aggressive",
      "personality": {
        "aggression": 0.9,
        "defense": 0.3,
        "harassment": 0.2
      }
    }
  ]
}
```

**Strategy values**: `"balanced"`, `"aggressive"`, `"defensive"`, `"expansionist"`, `"economic"`, `"harasser"`, `"rusher"`

**Note**: Skirmish mode uses the default `"balanced"` strategy if no strategy is specified.

## Debugging

Access debug metrics via `AIContext::debug_info`:
- `total_decisions_made`: Count of decision cycles
- `total_commands_issued`: Count of commands generated
- `state_transitions`: Count of state changes
- `deadlock_recoveries`: Count of forced recoveries

## Code Conventions

- **Variables/Functions**: `snake_case` (e.g., `total_units`, `update_context`)
- **Types**: `PascalCase` (e.g., `AIContext`, `BehaviorPriority`)
- **Thread Safety**: Mutex-protected queues, atomic busy flags

## Future Extensions

Potential improvements:
1. Hierarchical planning with strategic goals
2. Adaptive learning from outcomes
3. Active scouting behavior
4. Economic management
5. Team coordination
6. Dynamic strategy switching based on game state
7. More granular personality controls

## Implementation Details

### Strategy Impact on State Machine

Strategies modify the thresholds used in the state machine transitions:

```cpp
// Example: Aggressive AI attacks with fewer units
int MIN_UNITS_FOR_ATTACK = static_cast<int>(4.0F * strategy.min_attack_force);
// With aggressive strategy (min_attack_force = 0.6), needs only ~2 units
// With defensive strategy (min_attack_force = 1.5), needs ~6 units

// Example: Retreat threshold
if (ctx.average_health < ctx.strategy_config.retreat_threshold) {
  ctx.state = AIState::Retreating;
}
// Aggressive: retreats at 15% health
// Defensive: retreats at 40% health
```

### Production Rate Modifier

Economic strategies produce units faster:
```cpp
float production_interval = 1.5F / strategy_config.production_rate_modifier;
// Economic strategy (1.5x): produces every 1.0s
// Balanced (1.0x): produces every 1.5s
// Rusher (0.9x): produces every 1.67s
```

See the full implementation in `game/systems/ai_system/` directory.
