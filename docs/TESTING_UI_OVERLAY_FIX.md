# Testing UI Overlay Cleanup Fix

## Issue
After reloading a map or starting a new skirmish when one was already running, some parts of the UI overlay (specifically the victory/defeat screen) were not properly cleared, leaving a gray/shadow overlay visible.

## Fix Applied
Added a `Connections` block in `HUDVictory.qml` that listens for the `victoryStateChanged` signal from the game engine. When the victory state becomes empty (indicating a new game has started), the overlay state is reset by:
- Setting `showingSummary = false`
- Setting `battleSummary.visible = false`

## Manual Test Scenarios

### Scenario 1: New Skirmish After Victory
1. Start the game
2. Select a skirmish map and start the game
3. Play until victory or defeat (you can destroy enemy barracks or let them destroy yours)
4. Wait for the victory/defeat overlay to appear
5. Click "Continue" to see the battle summary
6. Click "Return to Menu"
7. Start a new skirmish from the main menu
8. **Expected**: No residual gray overlay should be visible; screen should be completely clear
9. **Pass/Fail**: _______

### Scenario 2: Load Game After Victory
1. Start the game
2. Select a skirmish map and start the game
3. Play until victory or defeat
4. Wait for the victory/defeat overlay to appear
5. Press ESC to return to main menu (or click "Continue" then "Return to Menu")
6. Select "Load Game" from the main menu
7. Load any saved game
8. **Expected**: No residual overlay from the previous game; screen should be clear
9. **Pass/Fail**: _______

### Scenario 3: New Skirmish from Victory Screen (Quick Restart)
1. Start the game
2. Select a skirmish map and start the game
3. Play until victory or defeat
4. Wait for the victory/defeat overlay to appear (don't click continue)
5. Press ESC to return to main menu
6. Immediately start a new skirmish
7. **Expected**: Victory overlay should be completely cleared; no gray shadow visible
8. **Pass/Fail**: _______

### Scenario 4: Multiple Consecutive Games
1. Start a skirmish and complete it (win or lose)
2. Return to menu and start another skirmish
3. Complete the second game (win or lose)
4. Return to menu and start a third skirmish
5. **Expected**: Each transition should be clean with no overlay residue
6. **Pass/Fail**: _______

## Visual Indicators of Success
- ✓ No gray/semi-transparent overlay visible when starting a new game
- ✓ Screen is fully clear and responsive
- ✓ No interaction blocking
- ✓ HUD elements (top and bottom bars) are clearly visible without dimming
- ✓ Game view is at full brightness (not darkened by residual overlay)

## Visual Indicators of Failure
- ✗ Gray semi-transparent rectangle covering part or all of the screen
- ✗ Screen appears dimmed or shadowed
- ✗ Victory/defeat text or buttons from previous game still visible
- ✗ Battle summary from previous game still visible
- ✗ Click events not registering (blocked by invisible overlay)

## Code Changes
- **File**: `ui/qml/HUDVictory.qml`
- **Change**: Added Connections block (lines 16-24) to watch for `victoryStateChanged` signal and reset overlay state when victoryState becomes empty

## Technical Notes
- The fix relies on the game engine properly emitting `victoryStateChanged` signal when `startSkirmish()` or `loadGameFromSlot()` is called
- The C++ side already clears `m_victoryState` in `VictoryService::configure()`, which is called by both game initialization methods
- The fix handles the QML-side state that was persisting across game sessions
