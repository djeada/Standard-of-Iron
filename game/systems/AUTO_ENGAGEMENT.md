# Melee Unit Auto-Engagement System

## Overview
The Combat System now includes automatic engagement logic for idle melee units. When a melee unit is idle and detects an enemy within its vision range, it will automatically issue an attack command to engage the enemy.

## Behavior

### When Auto-Engagement Triggers
Melee units will automatically engage enemies when **all** of the following conditions are met:

1. **Unit is melee-capable**: Has `AttackComponent` with `canMelee = true`
2. **Unit prefers melee**: Either `canRanged = false` OR `preferredMode = Melee`
   - This prevents ranged units (like archers) from running into melee range unnecessarily
3. **Unit is idle**: No active commands or actions
4. **Enemy is visible**: Enemy is within the unit's vision range
5. **No cooldown active**: Unit is not on engagement cooldown

### Idle State Definition
A unit is considered "idle" when it has:
- ❌ No attack target assigned (`AttackTargetComponent.targetId == 0`)
- ❌ No movement command active (`MovementComponent.hasTarget == false`)
- ❌ Not in hold mode (`HoldModeComponent.active == false`)
- ❌ Not in melee lock (`AttackComponent.inMeleeLock == false`)
- ❌ Not on patrol (`PatrolComponent.patrolling == false`)

### Target Selection
When auto-engaging, units will:
- Scan for enemies within **vision range** (from `UnitComponent.visionRange`)
- Target the **nearest enemy unit** (not buildings)
- Ignore **allied** and **neutral** units
- Set `shouldChase = true` to pursue the target

### Cooldown Mechanism
To prevent rapid re-engagement flicker:
- After auto-engaging, a unit enters a **0.5 second cooldown**
- During cooldown, the unit will not attempt to auto-engage again
- Cooldown is per-unit and is updated each frame

## Implementation Details

### Files Modified
- `game/systems/combat_system.h` - Added methods and cooldown tracking
- `game/systems/combat_system.cpp` - Implemented auto-engagement logic

### Key Methods

#### `processAutoEngagement(World*, float)`
Main auto-engagement logic that:
1. Updates cooldown timers
2. Iterates through all units
3. Checks eligibility for auto-engagement
4. Issues attack commands for idle melee units

#### `isUnitIdle(Entity*)`
Determines if a unit is idle by checking:
- Hold mode status
- Attack target assignment
- Movement commands
- Melee lock status
- Patrol status

#### `findNearestEnemy(Entity*, World*, float)`
Finds the nearest enemy within range:
- Searches within vision range
- Excludes allies and buildings
- Returns closest valid target

### Performance Considerations
- Uses **localized proximity checks** (vision range only)
- Avoids full world scans
- Cooldown prevents excessive re-checks
- Event-driven approach with component queries

## Unit Type Behavior

### Knights
- `canMelee = true`, `canRanged = false`, `preferredMode = Melee`
- **✅ WILL auto-engage** enemies within vision range when idle

### Archers
- `canMelee = true`, `canRanged = true`, `preferredMode = Auto`
- **❌ WILL NOT auto-engage** (prevents unnecessary melee charges)
- Archers still engage in melee if enemies close to melee range during combat

## Formation Integration
Auto-engagement respects formation integrity:
- Units in **formations** are typically not idle (have movement/patrol commands)
- Only units that complete their formation movement and become idle will auto-engage
- Formation cohesion is maintained as the system only acts on idle units

## Player Command Interaction
Auto-engagement never overrides player commands:
- **Move commands** prevent auto-engagement (unit is not idle)
- **Attack commands** prevent auto-engagement (unit has attack target)
- **Hold mode** prevents auto-engagement (checked in idle logic)
- **Patrol commands** prevent auto-engagement (unit is on patrol)

Only when a unit completes its command and becomes idle will it auto-engage.

## AI Interaction
AI-controlled units benefit from auto-engagement:
- AI systems issue high-level commands (attack, defend, gather)
- When AI units complete commands and become idle, they auto-engage
- Provides more responsive battlefield behavior for AI forces

## Edge Cases Handled
1. **Dead units** - Skipped (health <= 0 check)
2. **Buildings** - Excluded from auto-engagement targets
3. **Pending removal** - Skipped to avoid acting on units about to despawn
4. **Allies** - Never targeted (team and alliance checks)
5. **Rapid switching** - Prevented by cooldown mechanism
6. **Formation breaking** - Prevented by idle checks
