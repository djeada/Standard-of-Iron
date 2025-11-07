# Horse Animation Controller

## Overview

The `HorseAnimationController` provides a high-level API for controlling horse animations, including gaits, movements, and special animations. It encapsulates the complexity of phase/bob/timing calculations and integrates with rider context for coordinated animations.

## Usage Example

```cpp
#include "render/horse/horse_animation_controller.h"
#include "render/horse/rig.h"

// Create a horse profile
QVector3D leatherBase(0.5F, 0.4F, 0.3F);
QVector3D clothBase(0.7F, 0.2F, 0.1F);
HorseProfile profile = makeHorseProfile(12345, leatherBase, clothBase);

// Set up animation inputs and rider context
AnimationInputs anim;
anim.time = currentTime;
anim.isMoving = true;

HumanoidAnimationContext riderCtx;
// ... configure rider context

// Create controller
HorseAnimationController controller(profile, anim, riderCtx);

// Control gaits
controller.setGait(GaitType::TROT);
controller.updateGaitParameters();

// Accelerate/decelerate
controller.accelerate(2.0F);  // Increase speed by 2.0 units
controller.decelerate(1.0F);  // Decrease speed by 1.0 units

// Movement controls
controller.turn(0.5F, 0.3F);  // Turn with yaw angle and banking
controller.strafeStep(true, 1.0F);  // Step left

// Special animations
controller.rear(0.8F);  // Rear up to 80% height
controller.kick(true, 0.9F);  // Kick with rear legs at 90% power
controller.buck(0.7F);  // Buck with 70% intensity
controller.jumpObstacle(1.5F, 3.0F);  // Jump 1.5 units high, 3.0 units distance

// Query state
float phase = controller.getCurrentPhase();
float bob = controller.getCurrentBob();
float cycle = controller.getStrideCycle();
```

## Gait Types

- **IDLE**: Horse is standing still with gentle bobbing
- **WALK**: Slow four-beat gait (~1.5 units/sec)
- **TROT**: Moderate two-beat diagonal gait (~4.0 units/sec)
- **CANTER**: Fast three-beat gait (~6.5 units/sec)
- **GALLOP**: Fastest four-beat gait (~10.0 units/sec)

## Automatic Gait Transitions

The controller can automatically adjust gaits based on speed:
- Speed < 0.5: IDLE
- Speed 0.5-3.0: WALK
- Speed 3.0-5.5: TROT
- Speed 5.5-8.0: CANTER
- Speed > 8.0: GALLOP

## Integration with Rider

The controller integrates with `HumanoidAnimationContext` to:
- Synchronize phase with rider's gait cycle
- Calculate rein tension based on rider state
- Adjust bob intensity based on rider movement speed
- Respond to rider attack state

## Benefits

1. **Single Source of Truth**: All horse animation parameters managed in one place
2. **Easy Extension**: Add new gaits or special animations by extending the controller
3. **Automatic Calculations**: Phase, bob, and timing handled automatically
4. **Realistic Parameters**: Gait parameters based on real horse biomechanics
5. **Rider Integration**: Seamless coordination between horse and rider animations
