# Formation System Manual Testing Guide

## Overview
This guide describes how to manually test the multi-unit formation system in the Standard of Iron game.

## Prerequisites
- Game must be built and running
- A map with at least 2 players (you and AI) loaded
- Multiple units available for selection

## Test Scenarios

### 1. Basic Formation Mode Toggle

**Steps:**
1. Select a single unit
2. Observe the Formation button in the HUD (bottom panel)
   - **Expected:** Button should be disabled/grayed out
   - **Reason:** Formation requires 2+ units

3. Select 2 or more units (click-drag to select multiple, or Shift+click)
4. Observe the Formation button
   - **Expected:** Button should be enabled (clickable)

5. Click the Formation button
   - **Expected:** Button highlights with green color and shows checkmark (✓)
   - **Expected:** Units now have formation mode active

6. Click the Formation button again
   - **Expected:** Formation mode toggles off (no checkmark)

### 2. Formation Positioning - Roman Nation

**Steps:**
1. Play as Roman Republic (or select Roman units)
2. Select 6-9 Roman units
3. Enable Formation mode (click Formation button)
4. Right-click on empty ground to move units
   - **Expected:** Units arrange in a tight rectangular grid pattern
   - **Expected:** Units maintain consistent spacing (approximately 1.2x base spacing)
   - **Expected:** Formation is relatively compact and organized

5. Compare with non-formation movement:
   - Disable formation mode
   - Right-click to move to a new location
   - **Expected:** Units spread in a simple square grid (less organized)

### 3. Formation Positioning - Carthage Nation

**Steps:**
1. Play as Carthage (or select Carthage units)
2. Select 6-9 Carthage units
3. Enable Formation mode
4. Right-click on empty ground to move units
   - **Expected:** Units arrange in a looser rectangular formation
   - **Expected:** Positions have slight randomness/jitter (not perfectly aligned)
   - **Expected:** Formation is less rigid than Roman formation

### 4. Mixed Nation Units

**Steps:**
1. Select units from different nations (if possible in your scenario)
2. Enable Formation mode
3. Right-click to move
   - **Expected:** Units fall back to simple spread formation
   - **Expected:** No special nation-specific arrangement

### 5. Formation Mode Persistence

**Test that formation mode resets on various actions:**

#### 5a. Reset on Movement
1. Select multiple units, enable formation mode
2. Right-click to move units
   - **Expected:** Formation mode automatically disables after movement starts
   - **Expected:** Formation button no longer shows checkmark

#### 5b. Reset on Attack Command
1. Select multiple units, enable formation mode
2. Click Attack button, then click an enemy unit
   - **Expected:** Formation mode resets
   - **Expected:** Units attack the target

#### 5c. Reset on Stop Command
1. Select multiple units, enable formation mode
2. Click Stop button
   - **Expected:** Formation mode resets
   - **Expected:** Units stop moving

#### 5d. Reset on Hold Command
1. Select multiple units, enable formation mode
2. Click Hold button
   - **Expected:** Formation mode resets
   - **Expected:** Units enter hold mode instead

#### 5e. Reset on Guard Command
1. Select multiple units, enable formation mode
2. Click Guard button
   - **Expected:** Formation mode resets
   - **Expected:** Units enter guard mode

### 6. Formation with Different Unit Types

**Test with mixed unit types from same nation:**

1. Select 3 swordsmen + 3 archers (same nation)
2. Enable Formation mode
3. Right-click to move
   - **Expected:** All units use the same nation formation
   - **Expected:** No special front/back arrangement (this is phase 1)

2. Select cavalry only
3. Enable Formation mode
4. Right-click to move
   - **Expected:** Cavalry units form in nation-specific pattern

### 7. Large Formation Scaling

**Test with many units:**

1. Select 16+ units
2. Enable Formation mode
3. Right-click to move
   - **Expected:** Formation scales up (more rows/columns)
   - **Expected:** Spacing increases automatically for large groups
   - **Expected:** All units receive target positions

### 8. UI Visual Feedback

**Verify UI elements:**

1. Formation button appearance when disabled
   - Should be grayed with darker colors
   - Tooltip: "Select multiple units to use formation"

2. Formation button appearance when enabled but inactive
   - Normal colors (teal/green theme)
   - No checkmark
   - Tooltip: "Arrange units in tactical formation"

3. Formation button appearance when active
   - Bright green/teal highlight
   - Checkmark (✓) visible
   - Thicker border
   - Tooltip: "Exit formation mode (toggle)"

4. Selection count tracking
   - Button only enables when 2+ units selected
   - Updates immediately as selection changes

## Known Limitations

1. **Formation mode is temporary** - It resets whenever units receive new commands
2. **No persistent formations** - Units don't maintain formation while moving
3. **Mixed nations** - Falls back to simple spread for mixed-nation selections
4. **No role-based positioning** - Archers don't automatically go behind melee units (future enhancement)

## Troubleshooting

**Formation button doesn't enable:**
- Ensure you have 2+ units selected
- Check that selected units are actual combat units (not buildings)

**Formation doesn't look different:**
- Verify formation mode is active (checkmark visible)
- Try with more units (6-9) to see pattern clearly
- Compare same-nation units vs mixed units

**Formation mode doesn't reset:**
- This might be a bug - report with reproduction steps
- Check that you're using supported commands (move, attack, stop, hold, guard)

## Performance Notes

- Formation calculations are lightweight
- Suitable for groups up to 100+ units
- Single-pass iteration minimizes overhead
- Falls back gracefully on errors
