# Bug Fix: AI Unit Pullback on Recruitment

## Problem Description
When the AI recruited new troops, existing units that were already moving toward the rally point would pull back and regroup, causing inefficient movement patterns.

## Root Cause
The original implementation checked if each unit was within 15 world units of **its individual formation slot position**. When a new unit joined:

1. Formation positions were recalculated with more units
2. All formation slot positions shifted (e.g., slot 0 in a 5-unit formation ≠ slot 0 in a 6-unit formation)
3. Units already near their old slot were now far from their new slot
4. These units were marked as "distant" and triggered group pathfinding
5. This caused units to pull back toward the group centroid before advancing

## Solution
Changed distance check from **individual formation slot** to **group centroid** (average of all target positions).

### Before:
```cpp
QVector3D targetPos(member.target.x(), 0.0f, member.target.z());
float distanceToTargetSq = (targetPos - currentPos).lengthSquared();
```

### After:
```cpp
QVector3D groupCentroid(0.0f, 0.0f, 0.0f);
for (const auto &member : members)
    groupCentroid += member.target;
groupCentroid /= static_cast<float>(members.size());

// For each unit:
float distanceToCentroidSq = (groupCentroid - currentPos).lengthSquared();
```

## Impact
- Units already near the rally point (< 15 units from centroid) continue moving directly to their formation positions
- Only truly distant units (newly spawned or far away) use group pathfinding
- Formation recalculation no longer causes unnecessary pullbacks
- AI movement appears more natural and efficient

## Example Scenario

**Before Fix:**
1. 5 AI archers at rally point (positions 0-4)
2. New archer spawns → AI recalculates formation for 6 units
3. Position 0 shifts from (50, 50) to (49, 51)
4. Unit at old position thinks it's "distant" from new position
5. Unit pulls back to regroup

**After Fix:**
1. 5 AI archers near rally point centroid at (50, 50)
2. New archer spawns at barracks (20, 20)
3. Formation recalculated → centroid barely shifts to (48, 48)
4. Old units still within 15 units of centroid → move directly to new slots
5. Only new archer (>15 units from centroid) uses group pathfinding

## Commit
Fixed in commit 60a54af
