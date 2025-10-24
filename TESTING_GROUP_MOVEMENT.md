# Testing Group Movement Improvements

## Overview
This document describes how to manually test the improved group movement logic that prevents unnecessary regrouping pullbacks.

## Test Scenarios

### Scenario 1: Mixed Distance Group Movement
**Purpose:** Verify that units near the target move directly while distant units regroup.

**Setup:**
1. Start any map (e.g., map_forest.json)
2. Use selection to create a group of units scattered across different distances from a target point
3. Position some units within 15 world units of the target
4. Position other units beyond 15 world units from the target

**Test Steps:**
1. Select all units in the scattered group (use area selection)
2. Issue a move command to a distant target location
3. Observe the movement behavior

**Expected Results:**
- Units already within 15 units of target should move directly to their destination
- Units beyond 15 units should coordinate and move together as a group
- No unit that is already close to the target should pull back to regroup
- Distant units should still maintain formation coordination

### Scenario 2: All Units Nearby
**Purpose:** Verify behavior when all units are close to target.

**Test Steps:**
1. Select multiple units all positioned within 15 units of the target
2. Issue a group move command to the nearby target

**Expected Results:**
- All units should move directly to their destinations
- No group pathfinding should be triggered
- Movement should be immediate without coordination delay

### Scenario 3: All Units Distant
**Purpose:** Verify that group coordination is preserved for distant units.

**Test Steps:**
1. Select multiple units all positioned beyond 15 units from the target
2. Issue a group move command to the distant target

**Expected Results:**
- All units should coordinate and move together
- Group pathfinding should be used
- Formation should be maintained during movement

### Scenario 4: AI Group Movement
**Purpose:** Verify that AI-controlled groups also benefit from the improved logic.

**Test Steps:**
1. Start a match with AI opponents
2. Observe AI unit movements when they issue group commands
3. Check if AI units near their targets move directly

**Expected Results:**
- AI behavior should mirror player behavior
- AI units close to targets should not pull back unnecessarily
- AI distant units should still coordinate properly

## Configuration

### Threshold Value
The distance threshold is configured in `game/systems/command_service.h`:
```cpp
static constexpr float GROUP_REGROUP_DISTANCE_THRESHOLD = 15.0f;
```

This value can be adjusted for different gameplay balance. Lower values result in more direct movement, higher values result in more coordinated group movement.

## Performance Validation

### Expected Behavior:
- No increase in pathfinding requests (may actually decrease)
- No performance degradation in large unit groups
- No increase in CPU usage during group movement

### Monitoring Points:
- Watch for units oscillating or stuttering during movement
- Check that units don't repeatedly change targets
- Verify smooth transitions between direct and group movement modes

## Regression Checks

Ensure the following still work correctly:
- [ ] Formation logic for coordinated groups
- [ ] Pathfinding for distant units
- [ ] Attack-move commands
- [ ] Patrol commands
- [ ] Mixed unit type groups (archers, knights, spearmen)
- [ ] Collision avoidance
- [ ] Building collision detection
- [ ] Hold mode interactions
- [ ] Melee lock interactions

## Known Behaviors

### By Design:
- Units within 15 units use individual pathfinding (may take slightly different routes)
- Transition is instantaneous when crossing the 15-unit threshold
- Each unit in "nearby" group moves independently to its formation position

### Not Changed:
- Direct path threshold (8 units) for simple straight-line movement
- Waypoint following behavior
- Attack-move coordination
- Chase behavior during combat
