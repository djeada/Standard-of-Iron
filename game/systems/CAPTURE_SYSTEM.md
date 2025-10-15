# Barrack Capture System Documentation

## Overview
The barrack capture system allows players to take control of neutral or enemy barracks by maintaining a sufficient troop presence near them. This system includes visual feedback through progress bars and flag animations.

## Components

### CaptureComponent
Added to barracks to track capture progress:
- `capturingPlayerId`: ID of the player attempting capture (-1 if none)
- `captureProgress`: Time accumulated towards capture (seconds)
- `requiredTime`: Total time needed to complete capture (default: 5.0 seconds)
- `isBeingCaptured`: Boolean flag indicating active capture

### BarrackCapturedEvent
Event published when a barrack is captured:
- `barrackId`: Entity ID of the captured barrack
- `previousOwnerId`: Previous owner ID
- `newOwnerId`: New owner ID

## Capture Mechanics

### Requirements
To initiate capture, a player must have:
1. **3× troop advantage** within the capture radius
2. Continuous presence for the `requiredTime` duration

### Capture Radius
- Default: **8.0 units** from barrack center
- All troops within this radius are counted

### Capture Process
1. System checks for troops near each barrack every frame
2. Counts troops for all players within capture radius
3. If any player has 3× more troops than defenders:
   - Capture progress accumulates
   - Visual progress bar appears
   - Flag begins color transition animation
4. When `captureProgress` reaches `requiredTime`:
   - Ownership transfers to capturing player
   - Production component added (if capturing to non-neutral)
   - Building color updated
   - BarrackCapturedEvent published

### Capture Interruption
If troop advantage is lost:
- Progress decays at 2× the accumulation rate
- Visual indicators fade
- If progress reaches zero, capture attempt resets

## Visual Feedback

### Progress Bar
- Appears above the barrack during capture
- Golden/yellow color (RGB: 0.95, 0.75, 0.15)
- Shows percentage of capture completion
- Position: Above health bar

### Flag Animation
During capture, the flag:
1. **Lowers** progressively (up to 30% of height)
2. **Color transitions** from current owner to capturing player
3. **Size shrinks** slightly (up to 20% reduction)
4. Returns to normal when capture completes

### Color Coding
- **Neutral barracks**: Gray (RGB: 0.5, 0.5, 0.5)
- **Player-owned**: Team color from `teamColorForOwner()`
- **Transitioning**: Interpolated between old and new colors

## Integration

### CaptureSystem
- Runs every frame as part of the game loop
- Processes all barracks with BuildingComponent
- Automatically adds CaptureComponent if missing
- Updates ownership through BuildingCollisionRegistry

### Production Component Management
On capture:
- **Neutral → Player**: Adds ProductionComponent with default settings
  - Product type: "archer"
  - Build time: 10.0 seconds
  - Max units: 150
  - Rally point: 4 units east, 2 units south of barrack
- **Player → Neutral**: Removes ProductionComponent
- **Player → Player**: Keeps existing ProductionComponent

### AI Integration
The AI system can trigger captures automatically:
- During attack behaviors when troops cluster near barracks
- No specific capture AI behavior yet (future enhancement)
- Works with existing attack and movement commands

## Serialization
CaptureComponent state is fully serialized:
- Save games preserve ongoing capture attempts
- Progress and capturing player ID are restored
- Compatible with existing save/load system

## Testing

### Test Map
`assets/maps/barrack_capture_test.json` provides:
- Player 1 with 6 archers near neutral barrack at (50, 60)
- Player 2 with 3 archers, insufficient for capture
- Multiple barracks for testing various scenarios

### Debug Logging
Capture events print to console:
```
[Capture] Barrack <ID> captured! Previous owner: <X>, New owner: <Y>
```

## Configuration Constants

All constants in `CaptureSystem::processBarrackCapture()`:
```cpp
constexpr float CAPTURE_RADIUS = 8.0f;           // Detection radius
constexpr int TROOP_ADVANTAGE_MULTIPLIER = 3;    // Required superiority
```

In `CaptureComponent`:
```cpp
float requiredTime = 5.0f;  // Time to capture (seconds)
```

## Future Enhancements
- Dedicated AI capture behavior
- Configurable capture requirements per map
- Capture prevention mechanics (garrison troops)
- Capture speed modifiers based on troop types
- Audio cues for capture start/complete
- UI notifications for capture events
