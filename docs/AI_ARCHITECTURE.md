# How the AI System Actually Works

Imagine you're playing against four AI opponents, each controlling hundreds of units. They need to decide when to attack, when to defend, when to retreat, and when to expand—all while the player is actively trying to destroy them. And they need to do this without lagging the game, even on modest hardware.

This is the story of how Standard of Iron's AI thinks, decides, and acts. We'll follow an AI player through a complete decision cycle, from observing the battlefield to issuing commands to its troops.

## What we'll cover

We'll start with the philosophy behind the design: why centralized decisions, why throttled updates, why background threads. Then we'll trace the data flow from world state to AI commands. We'll look at how behaviors compete for control, how strategies shape personality, how commanders get used, and how the whole thing avoids getting stuck in deadlocks. Finally, we'll cover debugging, tuning, and how to add new behaviors.


## The core insight: think less, think smarter

Most game AI tutorials start with per-unit decision making. Each soldier has a behavior tree, each runs every frame. This works fine for 20 units. At 500 units per player times 4 AI players, you're running 2000 behavior trees every frame. That's not going to fly.

Our solution is centralized decision making. One AI brain per player, not per unit. The brain observes all its units, makes strategic decisions, and issues commands. Units don't think—they follow orders.

But even one brain per player can be expensive if it runs every frame. The AI needs to evaluate threats, calculate distances, consider production queues, and weigh priorities. At 60 FPS, that's 16ms per frame shared between rendering, physics, gameplay, and AI. There's no room.

So we throttle. Each AI updates roughly every 300ms—about 3 times per second. That's enough to react to tactical situations without burning CPU. And we run the thinking on background threads, so even if an AI takes 50ms to decide, the main game loop doesn't stutter.

Here's the basic flow:

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         AI UPDATE CYCLE                                    │
│                                                                            │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐                │
│  │   SNAPSHOT   │────▶│    REASON    │────▶│   EXECUTE    │                │
│  │              │     │              │     │              │                │
│  │  Read world  │     │ Update state │     │ Run behaviors│                │
│  │  state into  │     │ machine,     │     │ that match   │                │
│  │  immutable   │     │ detect       │     │ current      │                │
│  │  data        │     │ threats      │     │ situation    │                │
│  └──────────────┘     └──────────────┘     └──────────────┘                │
│         │                    │                    │                        │
│         │                    │                    ▼                        │
│         │                    │           ┌──────────────┐                  │
│         │                    │           │   COMMANDS   │                  │
│         │                    │           │              │                  │
│         │                    │           │ Move units,  │                  │
│         │                    │           │ attack,      │                  │
│         │                    │           │ produce      │                  │
│         │                    │           └──────────────┘                  │
│         │                    │                    │                        │
│         ▼                    ▼                    ▼                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    BACKGROUND THREAD                                │   │
│  │                                                                     │   │
│  │  Main thread hands off a "job" and continues.                       │   │
│  │  Worker thread computes. Main thread picks up results next frame.   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

The snapshot is the key insight. We copy the world state into an immutable struct, hand it to a background thread, and let that thread think as long as it needs. The main thread continues rendering and handling input. When the worker finishes, the main thread picks up the commands on the next update and applies them.


## The architecture in pieces

Let's look at the files involved. Everything lives in [game/systems/ai_system](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system):

```
ai_system/
├── ai_types.h              # Core data structures: snapshot, context, commands
├── ai_snapshot_builder.cpp # Reads world state into AISnapshot
├── ai_reasoner.cpp         # Updates context, runs state machine
├── ai_executor.cpp         # Runs behaviors, produces commands
├── ai_worker.cpp           # Background thread wrapper
├── ai_command_applier.cpp  # Applies commands back to world
├── ai_command_filter.cpp   # Prevents command spam
├── ai_behavior_registry.h  # Sorted list of behaviors per AI player
├── ai_behavior.h           # Base class for behaviors
├── ai_strategy.cpp         # Strategy presets (aggressive, defensive, etc.)
├── ai_tactical.cpp         # Utility functions for combat math
├── ai_utils.h              # Shared utilities: claim_units, cleanup, distance
└── behaviors/              # Individual behavior implementations
    ├── attack_behavior.cpp
    ├── defend_behavior.cpp
    ├── retreat_behavior.cpp
    ├── production_behavior.cpp
    ├── gather_behavior.cpp
    ├── expand_behavior.cpp
    ├── builder_behavior.cpp
    └── commander_behavior.cpp
```

The main coordinator is [ai_system.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system.cpp) in the parent folder. It owns all the AI instances and orchestrates the update loop.


## Data structures: what the AI knows

The AI works with three core data structures: the snapshot (what it sees), the context (what it remembers), and commands (what it decides).

The snapshot is defined in [ai_types.h](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_types.h). It's an immutable copy of the world state, safe to read on a background thread:

```cpp
struct AISnapshot {
  int player_id = 0;
  std::vector<EntitySnapshot> friendly_units;
  std::vector<ContactSnapshot> visible_enemies;
  float game_time = 0.0F;
};
```

Each entity gets serialized into an `EntitySnapshot` with everything the AI might need: position, health, whether it's a building, whether it's a commander, what it's producing, and whether it has a movement target. The snapshot builder in [ai_snapshot_builder.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_snapshot_builder.cpp) walks through the world and copies this data.

The context persists between updates. It tracks things like which state the AI is in, how long it's been in that state, which units are assigned to which tasks, and metrics like average health and unit counts:

```cpp
struct AIContext {
  int player_id = 0;
  AIState state = AIState::Idle;
  float state_timer = 0.0F;

  std::vector<EntityID> military_units;
  std::vector<EntityID> buildings;
  std::vector<EntityID> commander_ids;  // Active commander entities
  EntityID primary_barracks = 0;

  int total_units = 0;
  int idle_units = 0;
  float average_health = 1.0F;
  bool barracks_under_threat = false;

  float base_pos_x = 0.0F;  // Barracks position, or densest unit cluster
  float base_pos_z = 0.0F;
  float rally_x = 0.0F;     // 5 units in front of base
  float rally_z = 0.0F;

  // Tracks which units are assigned to which behavior
  std::unordered_map<EntityID, UnitAssignment> assigned_units;

  // Deadlock detection
  int consecutive_no_progress_cycles = 0;
  float last_meaningful_action_time = 0.0F;

  // Strategy modifiers
  AIStrategyConfig strategy_config;
};
```

Commands are simple orders. Each command carries the entity IDs it applies to and whatever extra data that command type needs:

```cpp
enum class AICommandType {
  MoveUnits,               // Move a list of units to formation positions
  AttackTarget,            // Order units to attack a specific entity
  StartProduction,         // Queue a unit type in a barracks
  StartBuilderConstruction,// Send a builder to construct a building
  TriggerCommanderRally    // Activate the rally ability on a commander unit
};
```


## The state machine: strategic phases

The AI operates in one of six states: Idle, Gathering, Attacking, Defending, Retreating, and Expanding. The state machine in [ai_reasoner.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_reasoner.cpp) decides when to transition:

```
                              ┌─────────────┐
                              │    IDLE     │
                              │             │
                              │  Waiting    │
                              │  for units  │
                              └──────┬──────┘
                                     │ have units
                                     ▼
                              ┌─────────────┐
         base threatened ────▶│  GATHERING  │◀──── enough force
                              │             │
                              │  Rally at   │
                              │  barracks   │
                              └──────┬──────┘
                                     │ enemies visible
                    ┌────────────────┴────────────────┐
                    ▼                                 ▼
             ┌─────────────┐                   ┌─────────────┐
             │  DEFENDING  │                   │  ATTACKING  │
             │             │                   │             │
             │  Protect    │                   │  Push       │
             │  base       │                   │  enemy      │
             └──────┬──────┘                   └──────┬──────┘
                    │ threat cleared                  │ heavy losses
                    │                                 │
                    ▼                                 ▼
             ┌─────────────┐                   ┌─────────────┐
             │  GATHERING  │                   │  RETREATING │
             │             │                   │             │
             │  Regroup    │                   │  Fall back  │
             └─────────────┘                   └─────────────┘
                                                      │ safe
                                                      ▼
                                               ┌─────────────┐
                                               │  GATHERING  │
                                               └─────────────┘
```

The transitions aren't just time-based. The reasoner looks at actual game state: is the barracks under threat? How many enemies are visible? What's the average health of our units? Are we making progress?

From [ai_reasoner.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_reasoner.cpp):

```cpp
if ((ctx.barracks_under_threat || !ctx.buildings_under_attack.empty()) &&
    ctx.state != AIState::Defending) {
  ctx.state = AIState::Defending;
}
else if (ctx.visible_enemy_count > 0 && ctx.average_enemy_distance < 50.0F &&
         (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
  ctx.state = AIState::Defending;
}
```

The state influences which behaviors are allowed to run. Defending state enables DefendBehavior. Attacking state enables AttackBehavior. But some behaviors like ProductionBehavior run in any state.


## Behaviors: who does what

Behaviors are modular pieces of AI logic. Each behavior knows how to do one thing: attack enemies, defend the base, retreat when damaged, produce units, and so on.

The base class in [ai_behavior.h](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_behavior.h) is simple:

```cpp
class AIBehavior {
public:
  virtual void execute(const AISnapshot &snapshot, AIContext &context,
                       float delta_time,
                       std::vector<AICommand> &outCommands) = 0;

  virtual auto should_execute(const AISnapshot &snapshot,
                              const AIContext &context) const -> bool = 0;

  virtual auto get_priority() const -> BehaviorPriority = 0;

  virtual auto can_run_concurrently() const -> bool { return false; }
};
```

Each update cycle, the executor checks which behaviors should run (based on state and situation), sorts them by priority, and executes them. Some behaviors are exclusive—only one can run. Others are concurrent—ProductionBehavior can queue units while AttackBehavior is moving troops.

Here are the behaviors and their priorities:

| Behavior | Priority | Concurrent? | What it does |
|----------|----------|-------------|--------------|
| RetreatBehavior | Critical | No | Fall back when health is low |
| DefendBehavior | Critical | No | Protect base from threats |
| CommanderBehavior | High | **Yes** | Position commanders, trigger rally ability |
| ProductionBehavior | High | Yes | Keep barracks producing |
| BuilderBehavior | High | Yes | Construct new buildings |
| ExpandBehavior | Normal | No | Capture neutral buildings |
| AttackBehavior | Normal | No | Push into enemy territory |
| GatherBehavior | Low | No | Rally scattered units |

Note that `CommanderBehavior`, `ProductionBehavior`, and `BuilderBehavior` are concurrent: they run alongside whatever strategic behavior is active, so production and commander management never stop even during a full-scale assault.

Each AI player has its own independent registry created by the `populate_behavior_registry()` helper in [ai_system.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system.cpp):

```cpp
static void populate_behavior_registry(AIBehaviorRegistry &reg) {
  reg.register_behavior(std::make_unique<AI::RetreatBehavior>());
  reg.register_behavior(std::make_unique<AI::DefendBehavior>());
  reg.register_behavior(std::make_unique<AI::CommanderBehavior>());
  reg.register_behavior(std::make_unique<AI::ProductionBehavior>());
  reg.register_behavior(std::make_unique<AI::BuilderBehavior>());
  reg.register_behavior(std::make_unique<AI::ExpandBehavior>());
  reg.register_behavior(std::make_unique<AI::AttackBehavior>());
  reg.register_behavior(std::make_unique<AI::GatherBehavior>());
}
```

`register_behavior` inserts each behavior in descending priority order (using `std::lower_bound`), so the executor always visits the highest-priority behaviors first without any runtime sorting.


## Inside a behavior: AttackBehavior

Let's look at how a specific behavior works. AttackBehavior in [attack_behavior.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/behaviors/attack_behavior.cpp) handles offensive operations.

First, it splits friendly units into two groups: units already engaged in combat, and units ready for new orders:

```cpp
for (const auto &entity : snapshot.friendly_units) {
  if (entity.is_building) continue;
  if (entity.spawn_type == Game::Units::SpawnType::Builder) continue;

  if (isEntityEngaged(entity, snapshot.visible_enemies)) {
    engaged_units.push_back(&entity);
  } else {
    ready_units.push_back(&entity);
  }
}
```

If there are no ready units, there's nothing to command. If there are no visible enemies, the behavior switches to scouting mode—sending units in different directions to find targets.

When enemies are visible, the behavior picks a target (usually the closest enemy or the most valuable one), calculates a formation position, and issues move/attack commands:

```cpp
AICommand cmd;
cmd.type = AICommandType::AttackTarget;
cmd.target_id = best_target.id;
cmd.unit_ids = units_to_attack;
outCommands.push_back(cmd);
```

The behavior has internal timers to prevent spamming commands. It only issues new orders every 1.5 seconds or so, giving units time to actually move before being redirected.


## Inside a behavior: CommanderBehavior

Commanders are special hero units that can use a rally ability to boost nearby troops. Without dedicated management, AI commanders would just sit idle while the army fought.

`CommanderBehavior` in [commander_behavior.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/behaviors/commander_behavior.cpp) runs on a 2-second timer and does two things:

1. **Positioning**: places each commander 4 units behind the army centroid (the average position of all living military units), offset toward the enemy. This keeps commanders protected while still influencing the front line.

2. **Rally triggering**: every 5 seconds, issues a `TriggerCommanderRally` command for each commander. The command applier sets `commander->rally_requested = true` on the actual entity, which causes the game's rally system to fire the ability.

Because `can_run_concurrently()` returns `true`, this behavior runs in parallel with whatever strategic behavior is active (attacking, defending, etc.). Commanders are never claimed as part of a behavior's unit pool — the behavior only moves and activates them by ID.

`should_execute` returns `true` whenever `ctx.commander_ids` is non-empty. The context's `commander_ids` vector is populated by the reasoner from `is_commander` flags on entity snapshots.


## Strategies: AI personality

Different AI opponents should feel different. An aggressive AI should attack early with small forces. A defensive AI should turtle up and build a large army before engaging. A rusher should go all-in immediately.

Strategies are configured in [ai_strategy.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_strategy.cpp). Each strategy is a set of multipliers that modify behavior thresholds:

```cpp
case AIStrategy::Aggressive:
  config.aggression_modifier = 1.5F;    // Attack more often
  config.defense_modifier = 0.7F;       // Care less about defense
  config.expansion_priority = 0.8F;     // Less interested in expansion
  config.production_rate_modifier = 1.2F;  // Produce slightly faster
  config.min_attack_force = 0.6F;       // Attack with fewer units (60% of normal)
  config.retreat_threshold = 0.15F;     // Retreat only at 15% health
  break;

case AIStrategy::Defensive:
  config.aggression_modifier = 0.5F;    // Attack less often
  config.defense_modifier = 1.8F;       // Very defensive
  config.expansion_priority = 0.5F;     // Stay close to base
  config.production_rate_modifier = 1.3F;  // Build up forces
  config.min_attack_force = 1.5F;       // Need 150% normal force to attack
  config.retreat_threshold = 0.40F;     // Retreat at 40% health
  break;
```

The behaviors read these modifiers and adjust their logic. For example, AttackBehavior checks min_attack_force to decide when to engage:

```cpp
int min_units_for_attack = static_cast<int>(4.0F * strategy.min_attack_force);
// Aggressive: 4 * 0.6 = 2.4 → needs ~3 units
// Defensive: 4 * 1.5 = 6 → needs 6 units
```

Strategies can be set per player in mission files:

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


## The threading model

Each AI player has a dedicated worker thread managed by [ai_worker.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_worker.cpp). The main thread never blocks waiting for AI—it just checks if results are ready.

The worker class looks like this:

```cpp
class AIWorker {
public:
  auto try_submit(AIJob &&job) -> bool;  // Main thread calls this
  void drain_results(std::queue<AIResult> &out);  // Main thread picks up results
  auto busy() const noexcept -> bool;  // Check if still computing
  
private:
  void worker_loop();  // Runs on background thread
  
  std::thread m_thread;
  std::atomic<bool> m_shouldStop{false};
  std::atomic<bool> m_workerBusy{false};
  
  std::mutex m_jobMutex;
  std::condition_variable m_jobCondition;
  AIJob m_pendingJob;
  
  std::mutex m_resultMutex;
  std::queue<AIResult> m_results;
};
```

The main update loop in [ai_system.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system.cpp) orchestrates everything:

```cpp
void AISystem::update(Engine::Core::World *world, float delta_time) {
  m_total_game_time += delta_time;
  
  // Pick up finished results from all workers
  process_results(*world);

  for (auto &ai : m_aiInstances) {
    ai.update_timer += delta_time;

    // Not time to update yet
    if (ai.update_timer < m_update_interval) continue;

    // Worker still busy with previous job
    if (ai.worker->busy()) continue;

    // Build snapshot and submit new job
    AISnapshot snapshot = AISnapshotBuilder::build(*world, ai.context.player_id);
    snapshot.game_time = m_total_game_time;

    AIJob job;
    job.snapshot = std::move(snapshot);
    job.context = ai.context;
    job.delta_time = ai.update_timer;

    if (ai.worker->try_submit(std::move(job))) {
      ai.update_timer = 0.0F;
    }
  }
}
```

This design means AI thinking time doesn't affect frame rate. If an AI takes 50ms to decide, the game keeps running. The next frame picks up the result.


## Per-instance behavior registries: why it matters

Each `AIInstance` owns its own `AIBehaviorRegistry`, created fresh by `populate_behavior_registry()` when the AI player joins. This is not just a clean architecture choice—it's a correctness requirement.

Behaviors have timer state. `CommanderBehavior` has `m_update_timer` and `m_rally_timer`. `AttackBehavior` has `m_attackTimer`. If two AI players shared one registry (and therefore one set of behavior instances), their timers would interfere. Player A's attack timer would reset when Player B issued attack commands.

With per-instance registries, every AI player's behaviors are completely independent objects with their own state. There is no shared mutable state between AI players. The workers can run truly in parallel without any locks beyond the job queue mutex.

The registry pointer is stored as a `unique_ptr<AIBehaviorRegistry>` inside `AIInstance`. This gives the behavior objects a stable heap address that won't move if the `m_aiInstances` vector ever reallocates.


## Deadlock prevention

AI can get stuck. The state machine might oscillate. Units might be assigned to behaviors that never release them. The AI might get into a state where it has idle units but never issues commands.

We have several safety mechanisms:

Time-based deadlock detection: if the AI stays in the same state for 60 seconds without meaningful action, force a transition:

```cpp
if (ctx.state_timer > ctx.max_state_duration) {
  deadlock_detected = true;
}
```

Progress-based detection: if we have idle units but haven't issued commands in a while, something's wrong:

```cpp
float time_since_progress = snapshot.game_time - ctx.last_meaningful_action_time;
if (time_since_progress >= max_no_progress_duration && ctx.idle_units > 0) {
  deadlock_detected = true;
}
```

Retreat timeout: retreating behavior can't last forever. After 12 seconds, it ends automatically:

```cpp
if (m_retreatDuration > 12.0F) {
  context.state = AIState::Gathering;
  m_retreatDuration = 0.0F;
}
```

Unit assignment cleanup: when behaviors finish, they release their unit assignments. If a unit stays assigned to a defunct task for too long, it gets freed.


## Command filtering

Without filtering, the AI might spam the same command every update cycle. "Move unit 42 to position X" repeated 3 times per second creates a lot of noise and can cause units to stutter.

The command filter in [ai_command_filter.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_command_filter.cpp) tracks recent commands and deduplicates. If we issued a move command to the same unit within the last second, skip the duplicate.

From [ai_system.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system.cpp):

```cpp
auto filtered_commands = m_commandFilter.filter(result.commands, m_total_game_time);
AICommandApplier::apply(world, ai.context.player_id, filtered_commands);
```


## Performance characteristics

Some numbers to keep in mind:

| Metric | Value |
|--------|-------|
| Update frequency | 300ms per AI (configurable) |
| Thread count | 1 per AI player |
| Snapshot size | ~200 bytes per entity |
| CPU scaling | Linear with unit count |
| Memory per AI | Context + snapshot (few KB typical) |

The system handles 500+ units per player without problems. At that scale, the bottleneck is usually rendering, not AI.

You can tune the update interval at runtime:

```cpp
ai_system.set_update_interval(0.5F);  // Slower updates (2 Hz) for weak CPUs
ai_system.set_update_interval(0.2F);  // Faster updates (5 Hz) for snappier AI
```


## Debugging

The AIContext has a debug_info struct that tracks metrics:

```cpp
struct DebugInfo {
  int total_decisions_made = 0;
  int total_commands_issued = 0;
  int state_transitions = 0;
  int deadlock_recoveries = 0;
  float last_update_time = 0.0F;
  float total_cpu_time = 0.0F;
};
```

Common issues and where to look:

When the AI does nothing, check if the worker is busy (stuck computation), if the snapshot has any units (maybe all dead), or if the command filter is too aggressive. Add logging to AIExecutor::run to see which behaviors are executing.

When the AI acts erratically, check state transitions in the reasoner. Log ctx.state changes. An AI flipping rapidly between Attacking and Retreating suggests the thresholds are wrong.

When units ignore commands, the command applier might be failing silently. Check that entity IDs in commands are still valid (units might have died between snapshot and application).

When performance is bad, profile the worker thread. The reasoner's update_context walks through all entities—if there are thousands, this can take time. Consider sampling only a subset of enemy units for the snapshot.


## Adding a new behavior

Say you want to add a ScoutBehavior that sends lone units to explore the map.

First, create the files:

```cpp
// behaviors/scout_behavior.h
class ScoutBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context,
               float delta_time, std::vector<AICommand> &outCommands) override;
  
  auto should_execute(const AISnapshot &snapshot,
                      const AIContext &context) const -> bool override;
  
  auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Low;
  }
  
  auto can_run_concurrently() const -> bool override { return true; }
};
```

Implement `should_execute` to check if scouting makes sense (early game, no visible enemies, have spare units). Implement `execute` to pick a unit, pick a direction, and issue a move command.

Add it to the `populate_behavior_registry` helper in `ai_system.cpp`:

```cpp
static void populate_behavior_registry(AIBehaviorRegistry &reg) {
  // ... existing behaviors ...
  reg.register_behavior(std::make_unique<AI::ScoutBehavior>());
}
```

Add the new files to `game/CMakeLists.txt` under the behavior sources list so they get compiled.

The behavior will automatically run when its conditions are met, for every AI player independently.

One thing to keep in mind: `should_execute` is called every update cycle (every 300ms). Keep it cheap—a few comparisons is fine, but don't do full spatial queries there. Heavy computation belongs in `execute`, which only runs when `should_execute` returns `true`.


## Scalability: what works well, what to watch

This section is an honest assessment of the system's strengths and current limits.

**What scales well**

The thread-per-player model is surprisingly robust. With 8 AI players, you have 8 background threads, each blocked on a condition variable between updates. Modern OSes handle hundreds of sleeping threads cheaply. The 300ms throttle means each thread is active only ~0.3% of the time.

Per-instance registries eliminate all inter-player lock contention. When player A's `CommanderBehavior` updates its `m_rally_timer`, it touches only its own object. Nothing is shared.

The snapshot+context copy-on-submit design is also clean: main thread has the authoritative context, worker thread gets a full copy. No partial reads, no stale pointers. Commands come back as a plain vector and are applied safely on the main thread.

Adding new behaviors has O(1) impact on existing behavior performance. Each behavior only runs when its `should_execute` returns true, and non-concurrent behaviors skip the rest once one claims the run slot.

**Current limitations worth knowing**

*Perfect information*: the AI always sees all enemy units regardless of distance. There's no perception radius or fog-of-war equivalent for AI. This makes the AI feel omniscient in battles—it will always rally toward distant threats it couldn't realistically see. Adding a range filter in the snapshot builder is the right fix if you want believable AI.

*No inter-player coordination*: two allied AIs don't talk to each other. Each one decides independently. They might both retreat at the same time, or both attack the same target. The architecture doesn't block this—you could add an `AICoordinator` that reads from all contexts and writes suggestions into each one—but nothing implements it today.

*Context copy size*: the entire `AIContext` is copied into every job, including the `assigned_units` map. For 500 units with active assignments, this is ~30–50 KB of copying per AI player per 300ms tick. That's about 1.5 MB/s per player. Manageable for typical unit counts, but worth profiling at extreme scales.

*Three-vector float layout for positions*: move commands store target positions as three separate float vectors (`move_target_x`, `move_target_y`, `move_target_z`). This works but means three heap allocations per move command. If you add behaviors that issue many small move commands, consider a single vector of `glm::vec3` positions instead.

*Early-game cluster detection is O(N²)*: `densest_anchor_cluster` finds the tightest group of units to use as a base anchor when no barracks exists. This is O(N²) in unit count. It only runs pre-barracks with very few units, so in practice N is never more than 5–10. Not a real concern.

**Practical ceiling**: the system handles 8 AI players comfortably on modern hardware. Getting to 20+ players would require either snapshot streaming (send only changed entities rather than all entities) or reducing the context copy size. Neither is needed for the game's current design.


## Future improvements

The system is well-structured for growth in these directions:

**Hierarchical planning** could let a high-level planner set goals ("control the north side of the map", "pressure their economy") while behaviors execute the details. Right now the state machine is flat—there's no concept of strategic objectives above the individual behavior level.

**Adaptive strategy switching** could shift from Aggressive to Defensive mid-game based on measured outcomes. If three consecutive attacks failed, that's a signal to change approach. The `AIStrategyConfig` is already the right place to make this data-driven.

**Perception radius** would make the AI believable—only seeing enemies within a realistic sight distance. The snapshot builder is the right place to add this filter.

**Team coordination** between allied AI players would prevent doubling up. An `AICoordinator` component could read all AI contexts and write target suggestions so two allied AIs attack different flanks instead of the same one.

**Scripted event hooks** could let mission designers trigger behavior changes (e.g., "when player captures the bridge, all AIs switch to Aggressive"). A simple callback registry on `AISystem` would be enough.

The architecture supports all of these—behaviors are self-contained, strategies are data-driven, and the threading model is extensible. They're waiting for implementation time.


## Finding your way around

| What you want to do | Where to look |
|---------------------|---------------|
| Adjust AI update rate | [ai_system.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system.cpp) - set_update_interval() |
| Add a new strategy | [ai_strategy.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_strategy.cpp) - add case in create_config() |
| Create a new behavior | [behaviors/](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/behaviors) - new file, register in populate_behavior_registry(), add to CMakeLists |
| Change commander positioning | [commander_behavior.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/behaviors/commander_behavior.cpp) - k_protected_offset, k_rally_interval |
| Tune attack timing | [attack_behavior.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/behaviors/attack_behavior.cpp) - m_attackTimer threshold |
| Debug state transitions | [ai_reasoner.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_reasoner.cpp) - update_state_machine() |
| Understand what AI sees | [ai_snapshot_builder.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_snapshot_builder.cpp) - build() |
| Configure AI per mission | Mission JSON files - ai_setups section |
| Add a new AI command type | [ai_types.h](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/ai_system/ai_types.h) - AICommandType enum, then handle in ai_command_applier.cpp |

The code follows snake_case for variables and functions, PascalCase for types. All background thread access goes through mutexes or atomics. The snapshot is immutable once built. Commands are queued and applied on the main thread only.

When in doubt, start with `AISystem::update()` and follow the data flow: snapshot → worker → result → apply. That covers 90% of what the AI does.
