# RTS AI System Architecture

## Overview

The RTS AI system is designed for scalability, performance, and strategic behavior in large-scale battles. It uses a multi-threaded, behavior-based architecture with centralized decision-making.

## Core Design Principles

1. **Centralized Decision Logic**: All AI decisions are made in a central system, not per-unit
2. **Throttled Updates**: AI evaluates at fixed intervals (default 300ms), not every frame
3. **Asynchronous Processing**: AI reasoning runs on background threads to avoid blocking the main game loop
4. **Behavior-Based Actions**: Modular behaviors with priority system allow concurrent execution
5. **Deadlock Prevention**: Built-in timeout and recovery mechanisms prevent stuck states
6. **Snake Case Naming**: All code follows snake_case conventions for consistency

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

Update interval can be adjusted:
```cpp
ai_system.set_update_interval(0.5f); // Reduce to 2 Hz for performance
```

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

See the full implementation in `game/systems/ai_system/` directory.
