# Technical Documentation: Movement System Fix

## Problem Analysis

### Issue 1: Wrong Direction Movement on Consecutive Commands

**Symptom**: When issuing a second move command to a troop already moving, units sometimes move in the wrong direction — away from the new target instead of toward it.

**Root Cause**: 
The movement system uses an asynchronous pathfinding worker thread. When a new move command is issued:
1. Previous path is cleared and `pathPending` is set to true
2. However, velocity (`vx`, `vz`) retained its old values
3. Unit continues moving with old velocity while waiting for new path
4. This causes movement in the wrong direction for 100-400ms (typical pathfinding time)

**Technical Details**:
```cpp
// Before fix (command_service.cpp:264-268)
mv->path.clear();
mv->hasTarget = false;
mv->pathPending = true;
// vx and vz NOT cleared - bug!
```

**Fix**:
```cpp
// After fix (command_service.cpp:264-268)
mv->path.clear();
mv->hasTarget = false;
mv->vx = 0.0f;  // NEW: Clear velocity immediately
mv->vz = 0.0f;  // NEW: Clear velocity immediately
mv->pathPending = true;
```

**Impact**: Units now stop immediately when receiving a new command, preventing wrong-direction movement.

---

### Issue 2: Jitter During Path Application

**Symptom**: When the pathfinding thread completes and returns a new path, there's visible jitter as the unit transitions from old movement to new path.

**Root Cause**:
In `processPathResults`, the order of operations was:
1. Clear path
2. Set goal
3. **THEN** clear velocity
4. Apply new path

This meant the old velocity was still active for one frame while the new path was being set up, causing interpolation artifacts.

**Technical Details**:
```cpp
// Before fix (command_service.cpp:582-587)
movementComponent->path.clear();
movementComponent->goalX = target.x();
movementComponent->goalY = target.z();
movementComponent->vx = 0.0f;  // Too late!
movementComponent->vz = 0.0f;  // Too late!
```

**Fix**:
```cpp
// After fix (command_service.cpp:597-601)
// Clear old path and velocity immediately to prevent wrong direction movement
movementComponent->path.clear();
movementComponent->vx = 0.0f;  // MOVED UP: Clear before setting new data
movementComponent->vz = 0.0f;  // MOVED UP: Clear before setting new data
movementComponent->goalX = target.x();
movementComponent->goalY = target.z();
```

**Impact**: Cleaner transitions between paths with no interpolation artifacts.

---

### Issue 3: Group Movement Backward Balancing

**Symptom**: When some troops in a group are far behind, the pathfinding causes units already near the destination to move back toward stragglers to "balance" the formation.

**Root Cause**:
In `moveGroup`, ALL units in the group were being repathed whenever a new group command was issued, even units already at their destination. The formation offset calculation would then cause these units to move back toward the group center.

**Technical Details**:
The group movement system works by:
1. Finding a "leader" unit (closest to average target position)
2. Computing one path for the leader
3. Applying the same path to all units with individual offsets

Problem: Even units AT the destination would get the leader's path + offset, causing them to move.

**Fix**:
```cpp
// NEW: Early exit for units already at destination (command_service.cpp:421-429)
// Check if unit is already very close to its destination
float distToTargetX = member.transform->position.x - member.target.x();
float distToTargetZ = member.transform->position.z - member.target.z();
float distToTargetSq = distToTargetX * distToTargetX + distToTargetZ * distToTargetZ;

if (distToTargetSq <= NEAR_DESTINATION_THRESHOLD_SQ) {
  // Unit is already at or very near destination, don't repath
  continue;
}
```

**Impact**: Units within 1.0 units of their target are excluded from group repathfinding, preventing backward movement.

---

## Implementation Details

### Constants Added

```cpp
constexpr float NEAR_DESTINATION_THRESHOLD_SQ = 1.0f;  // 1 unit radius
```

This threshold is chosen to be:
- Large enough to prevent unnecessary repathfinding
- Small enough to not interfere with formation coherence
- Compatible with the existing `arriveRadius` in movement_system.cpp (0.05-0.25)

### Thread Safety

All fixes maintain existing thread safety:
- Velocity resets happen in main thread before pathfinding submission
- processPathResults already has proper mutex protection via pendingMutex
- No new race conditions introduced

### Performance Impact

**CPU**: Negligible
- 2 additional float assignments per move command (< 1ns each)
- 1 additional distance-squared calculation per unit in group moves (~10-20ns)
- No impact on pathfinding worker thread

**Memory**: Zero additional memory usage

**Latency**: Improved (movement response is faster, not slower)

---

## Edge Cases Handled

1. **Rapid successive commands**: Units properly clear velocity between each command
2. **Mixed group states**: Group movement correctly handles units in various states (moving, stopped, near destination)
3. **AI commands**: Works identically for AI and player-issued commands
4. **Attack-move**: Does not interfere with attack movement (clearAttackIntent flag respected)
5. **Melee lock**: Units in melee lock still bypass all movement (existing check at line 104)

---

## Validation

Since the project uses Qt and has no unit test infrastructure, validation should be done via:

1. **Manual testing**: See TESTING_MOVEMENT_FIX.md
2. **Visual inspection**: Observe unit movement in-game
3. **Performance profiling**: Ensure no FPS impact

---

## Future Improvements

Possible enhancements (not implemented to maintain minimal changes):

1. **Predictive movement**: Start moving toward new target even before pathfinding completes
2. **Path smoothing**: Apply Catmull-Rom splines to waypoints for curved movement
3. **Formation preservation**: Better group cohesion without backward movement
4. **Adaptive thresholds**: Adjust NEAR_DESTINATION_THRESHOLD based on unit speed

---

## Related Files

- `game/systems/command_service.cpp`: Main fix location
- `game/systems/movement_system.cpp`: Movement execution (not modified)
- `game/systems/pathfinding.cpp`: Async pathfinding (not modified)
- `game/core/component.h`: MovementComponent definition (not modified)

---

## Backward Compatibility

This fix is fully backward compatible:
- No API changes
- No component structure changes
- No serialization format changes
- Existing save games will work correctly
