# How the AI System Actually Works

Imagine you're playing against four AI opponents, each controlling hundreds of units. They need to decide when to attack, when to defend, when to retreat, and when to expand—all while the player is actively trying to destroy them. And they need to do this without lagging the game, even on modest hardware.

This is the story of how Standard of Iron's AI thinks, decides, and acts. We'll follow an AI player through a complete decision cycle, from observing the battlefield to issuing commands to its troops.

## What we'll cover

We'll start with the philosophy behind the design: why centralized decisions, why throttled updates, why background threads. Then we'll trace the data flow from world state to AI commands. We'll look at how behaviors compete for control, how strategies shape personality, and how the whole thing avoids getting stuck in deadlocks. Finally, we'll cover debugging and tuning.


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

Let's look at the files involved. Everything lives in [game/systems/ai_system](../game/systems/ai_system):

```
ai_system/
├── ai_types.h              # Core data structures: snapshot, context, commands
├── ai_snapshot_builder.cpp # Reads world state into AISnapshot
├── ai_reasoner.cpp         # Updates context, runs state machine
├── ai_executor.cpp         # Runs behaviors, produces commands
├── ai_worker.cpp           # Background thread wrapper
├── ai_command_applier.cpp  # Applies commands back to world
├── ai_command_filter.cpp   # Prevents command spam
├── ai_strategy.cpp         # Strategy presets (aggressive, defensive, etc.)
├── ai_behavior.h           # Base class for behaviors
├── ai_tactical.cpp         # Utility functions for combat math
└── behaviors/              # Individual behavior implementations
    ├── attack_behavior.cpp
    ├── defend_behavior.cpp
    ├── retreat_behavior.cpp
    ├── production_behavior.cpp
    ├── gather_behavior.cpp
    ├── expand_behavior.cpp
    └── builder_behavior.cpp
```

The main coordinator is [ai_system.cpp](../game/systems/ai_system.cpp) in the parent folder. It owns all the AI instances and orchestrates the update loop.


## Data structures: what the AI knows

The AI works with three core data structures: the snapshot (what it sees), the context (what it remembers), and commands (what it decides).

The snapshot is defined in [ai_types.h](../game/systems/ai_system/ai_types.h). It's an immutable copy of the world state, safe to read on a background thread:

```cpp
struct AISnapshot {
  int player_id = 0;
  std::vector<EntitySnapshot> friendly_units;
  std::vector<ContactSnapshot> visible_enemies;
  float game_time = 0.0F;
};
```

Each entity gets serialized into an EntitySnapshot with everything the AI might need: position, health, whether it's a building, what it's producing, whether it has a movement target. The snapshot builder in [ai_snapshot_builder.cpp](../game/systems/ai_system/ai_snapshot_builder.cpp) walks through the world and copies this data.

The context persists between updates. It tracks things like which state the AI is in, how long it's been in that state, which units are assigned to which tasks, and metrics like average health and unit counts:

```cpp
struct AIContext {
  int player_id = 0;
  AIState state = AIState::Idle;
  float state_timer = 0.0F;
  
  std::vector<EntityID> military_units;
  std::vector<EntityID> buildings;
  EntityID primary_barracks = 0;
  
  int total_units = 0;
  int idle_units = 0;
  float average_health = 1.0F;
  bool barracks_under_threat = false;
  
  // Tracks which units are assigned to which behavior
  std::unordered_map<EntityID, UnitAssignment> assigned_units;
  
  // Deadlock detection
  int consecutive_no_progress_cycles = 0;
  float last_meaningful_action_time = 0.0F;
  
  // Strategy modifiers
  AIStrategyConfig strategy_config;
};
```

Commands are simple orders: move these units here, attack this target, start producing this unit type:

```cpp
enum class AICommandType {
  MoveUnits,
  AttackTarget,
  StartProduction,
  StartBuilderConstruction
};
```


## The state machine: strategic phases

The AI operates in one of six states: Idle, Gathering, Attacking, Defending, Retreating, and Expanding. The state machine in [ai_reasoner.cpp](../game/systems/ai_system/ai_reasoner.cpp) decides when to transition:

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

From [ai_reasoner.cpp](../game/systems/ai_system/ai_reasoner.cpp):

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

The base class in [ai_behavior.h](../game/systems/ai_system/ai_behavior.h) is simple:

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
| ProductionBehavior | High | Yes | Keep barracks producing |
| BuilderBehavior | High | Yes | Construct new buildings |
| ExpandBehavior | Normal | No | Capture neutral buildings |
| AttackBehavior | Normal | No | Push into enemy territory |
| GatherBehavior | Low | No | Rally scattered units |

The behaviors are registered in [ai_system.cpp](../game/systems/ai_system.cpp) at startup:

```cpp
AISystem::AISystem() {
  m_behaviorRegistry.register_behavior(std::make_unique<AI::RetreatBehavior>());
  m_behaviorRegistry.register_behavior(std::make_unique<AI::DefendBehavior>());
  m_behaviorRegistry.register_behavior(std::make_unique<AI::ProductionBehavior>());
  m_behaviorRegistry.register_behavior(std::make_unique<AI::BuilderBehavior>());
  m_behaviorRegistry.register_behavior(std::make_unique<AI::ExpandBehavior>());
  m_behaviorRegistry.register_behavior(std::make_unique<AI::AttackBehavior>());
  m_behaviorRegistry.register_behavior(std::make_unique<AI::GatherBehavior>());
}
```


## Inside a behavior: AttackBehavior

Let's look at how a specific behavior works. AttackBehavior in [attack_behavior.cpp](../game/systems/ai_system/behaviors/attack_behavior.cpp) handles offensive operations.

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


## Strategies: AI personality

Different AI opponents should feel different. An aggressive AI should attack early with small forces. A defensive AI should turtle up and build a large army before engaging. A rusher should go all-in immediately.

Strategies are configured in [ai_strategy.cpp](../game/systems/ai_system/ai_strategy.cpp). Each strategy is a set of multipliers that modify behavior thresholds:

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

Each AI player has a dedicated worker thread managed by [ai_worker.cpp](../game/systems/ai_system/ai_worker.cpp). The main thread never blocks waiting for AI—it just checks if results are ready.

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

The main update loop in [ai_system.cpp](../game/systems/ai_system.cpp) orchestrates everything:

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

The command filter in [ai_command_filter.cpp](../game/systems/ai_system/ai_command_filter.cpp) tracks recent commands and deduplicates. If we issued a move command to the same unit within the last second, skip the duplicate.

From [ai_system.cpp](../game/systems/ai_system.cpp):

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

Implement should_execute to check if scouting makes sense (early game, no visible enemies, have spare units). Implement execute to pick a unit, pick a direction, and issue a move command.

Register it in AISystem::AISystem():

```cpp
m_behaviorRegistry.register_behavior(std::make_unique<AI::ScoutBehavior>());
```

The behavior will automatically run when its conditions are met.


## Future improvements

The current system is solid for RTS basics but has room to grow:

Hierarchical planning could set high-level goals ("control the north side of the map") and let behaviors figure out details. Right now the state machine is flat.

Adaptive learning could adjust strategy based on what's working. If aggressive attacks keep failing, become more defensive.

Better scouting would maintain a mental map of explored areas and send units to unexplored regions.

Team coordination for allied AI players would let them focus different objectives instead of all doing the same thing.

Dynamic strategy switching could change from Aggressive to Defensive mid-game based on relative strength.

The architecture supports all of these—behaviors are modular, strategies are data-driven, and the threading model can handle more computation. It's a matter of implementation time.


## Finding your way around

| What you want to do | Where to look |
|---------------------|---------------|
| Adjust AI update rate | [ai_system.cpp](../game/systems/ai_system.cpp) - set_update_interval() |
| Add a new strategy | [ai_strategy.cpp](../game/systems/ai_system/ai_strategy.cpp) - add case in create_config() |
| Create a new behavior | [behaviors/](../game/systems/ai_system/behaviors) - create new file, register in ai_system.cpp |
| Tune attack timing | [attack_behavior.cpp](../game/systems/ai_system/behaviors/attack_behavior.cpp) - m_attackTimer threshold |
| Debug state transitions | [ai_reasoner.cpp](../game/systems/ai_system/ai_reasoner.cpp) - update_state_machine() |
| Understand what AI sees | [ai_snapshot_builder.cpp](../game/systems/ai_system/ai_snapshot_builder.cpp) - build() |
| Configure AI per mission | Mission JSON files - ai_setups section |

The code follows snake_case for variables and functions, PascalCase for types. All background thread access goes through mutexes or atomics. The snapshot is immutable once built. Commands are queued and applied on the main thread only.

When in doubt, start with the AISystem::update() method and follow the data flow: snapshot → worker → result → apply. That covers 90% of what the AI does.
