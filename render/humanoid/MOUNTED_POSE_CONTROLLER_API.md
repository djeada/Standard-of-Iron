# MountedPoseController API

High-level API for mounted humanoid poses that handles all rider-mount attachment logic.

## Overview

The `MountedPoseController` class provides a comprehensive API for animating humanoid characters on horseback. It handles:
- Body positioning and IK for mounted poses
- Riding postures and animations
- Mounted combat animations
- Equipment interaction (reins, weapons, shields)

## Basic Usage

```cpp
#include "render/humanoid/mounted_pose_controller.h"

// Initialize pose and animation context
HumanoidPose pose;
HumanoidAnimationContext anim_ctx;

// Get horse mount frame (from horse renderer)
HorseMountFrame mount = compute_mount_frame(horse_profile);

// Create controller
MountedPoseController controller(pose, anim_ctx);

// Mount the rider on the horse
controller.mountOnHorse(mount);

// Set idle riding pose
controller.ridingIdle(mount);
```

## Mounting and Dismounting

```cpp
// Mount on horse
controller.mountOnHorse(mount);
// - Positions pelvis on saddle
// - Places feet in stirrups
// - Adjusts body height
// - Calculates knee positions

// Dismount
controller.dismount();
// - Resets to standing position
```

## Riding Postures

### Idle Riding
```cpp
controller.ridingIdle(mount);
// Hands rest on thighs or hold reins loosely
```

### Leaning
```cpp
// Lean forward (charging position)
controller.ridingLeaning(mount, 1.0f, 0.0f);

// Lean to the side (turning)
controller.ridingLeaning(mount, 0.0f, 0.5f);

// Combined lean
controller.ridingLeaning(mount, 0.8f, -0.3f);
```

### Charging
```cpp
// Aggressive forward lean with crouch
controller.ridingCharging(mount, 1.0f);  // Full intensity
controller.ridingCharging(mount, 0.5f);  // Half intensity
```

### Reining
```cpp
// Pull back on reins to stop/slow the horse
controller.ridingReining(mount, 1.0f, 1.0f);  // Both reins tight

// Turn left by pulling left rein
controller.ridingReining(mount, 1.0f, 0.3f);

// Turn right by pulling right rein
controller.ridingReining(mount, 0.3f, 1.0f);
```

## Mounted Combat

### Melee Strike
```cpp
// Animate a full melee attack from horseback
for (float phase = 0.0f; phase <= 1.0f; phase += deltaTime * 2.0f) {
    controller.ridingMeleeStrike(mount, phase);
    // phase 0.0-0.30: Windup (raise weapon)
    // phase 0.30-0.50: Strike (swing down)
    // phase 0.50-1.0: Recovery (return to rest)
}
```

### Spear Thrust
```cpp
// Animate a spear thrust attack
for (float phase = 0.0f; phase <= 1.0f; phase += deltaTime * 2.5f) {
    controller.ridingSpearThrust(mount, phase);
    // phase 0.0-0.25: Prepare (draw back)
    // phase 0.25-0.45: Thrust forward
    // phase 0.45-1.0: Retract
}
```

### Bow Shot
```cpp
// Animate drawing and releasing a bow
for (float phase = 0.0f; phase <= 1.0f; phase += deltaTime * 1.5f) {
    controller.ridingBowShot(mount, phase);
    // phase 0.0-0.30: Draw bow
    // phase 0.30-0.65: Hold aim
    // phase 0.65-1.0: Release and follow-through
}
```

### Shield Defense
```cpp
// Raise shield
controller.ridingShieldDefense(mount, true);

// Lower shield
controller.ridingShieldDefense(mount, false);
```

## Equipment Interaction

### Holding Reins
```cpp
// Tight reins (no slack)
controller.holdReins(mount, 0.0f, 0.0f);

// Loose reins (slack)
controller.holdReins(mount, 0.8f, 0.8f);

// Asymmetric for turning
controller.holdReins(mount, 0.2f, 0.7f);
```

### Spear Grips

```cpp
// Overhead grip for downward thrust
controller.holdSpearMounted(mount, SpearGrip::OVERHAND);

// Couched lance grip for charging
controller.holdSpearMounted(mount, SpearGrip::COUCHED);

// Two-handed grip on shaft
controller.holdSpearMounted(mount, SpearGrip::TWO_HANDED);
```

### Bow Ready
```cpp
// Hold bow in ready position while mounted
controller.holdBowMounted(mount);
```

## Complete Example: Mounted Archer

```cpp
void animateMountedArcher(float time, HumanoidPose& pose, 
                          const HumanoidAnimationContext& anim_ctx,
                          const HorseMountFrame& mount) {
    MountedPoseController controller(pose, anim_ctx);
    
    // Mount on horse
    controller.mountOnHorse(mount);
    
    // State machine for combat
    float attackTime = fmod(time, 3.0f);
    
    if (attackTime < 1.0f) {
        // Idle/riding phase
        controller.ridingIdle(mount);
        controller.holdBowMounted(mount);
    } else if (attackTime < 2.5f) {
        // Drawing and shooting phase
        float attackPhase = (attackTime - 1.0f) / 1.5f;
        controller.ridingBowShot(mount, attackPhase);
    } else {
        // Recovery phase
        controller.ridingIdle(mount);
        controller.holdBowMounted(mount);
    }
}
```

## Complete Example: Cavalry Charge

```cpp
void animateCavalryCharge(float chargeIntensity, 
                          HumanoidPose& pose,
                          const HumanoidAnimationContext& anim_ctx,
                          const HorseMountFrame& mount) {
    MountedPoseController controller(pose, anim_ctx);
    
    // Mount on horse
    controller.mountOnHorse(mount);
    
    // Charging posture
    controller.ridingCharging(mount, chargeIntensity);
    
    // Hold spear in couched position
    controller.holdSpearMounted(mount, SpearGrip::COUCHED);
}
```

## API Reference

### Mounting Methods
- `mountOnHorse(mount)` - Positions rider on horse
- `dismount()` - Returns rider to standing position

### Riding Posture Methods
- `ridingIdle(mount)` - Neutral riding position
- `ridingLeaning(mount, forward_lean, side_lean)` - Dynamic leaning (-1.0 to 1.0)
- `ridingCharging(mount, intensity)` - Aggressive charging posture (0.0 to 1.0)
- `ridingReining(mount, left_tension, right_tension)` - Pull reins (0.0 to 1.0)

### Combat Methods
- `ridingMeleeStrike(mount, attack_phase)` - Melee attack animation (0.0 to 1.0)
- `ridingSpearThrust(mount, attack_phase)` - Spear thrust animation (0.0 to 1.0)
- `ridingBowShot(mount, draw_phase)` - Bow shooting animation (0.0 to 1.0)
- `ridingShieldDefense(mount, raised)` - Shield position (true/false)

### Equipment Methods
- `holdReins(mount, left_slack, right_slack)` - Rein holding (0.0 to 1.0)
- `holdSpearMounted(mount, grip_style)` - Spear grip position
- `holdBowMounted(mount)` - Bow ready position

### Enum: SpearGrip
- `OVERHAND` - Spear overhead for thrusting down
- `COUCHED` - Spear under arm for charging (lance style)
- `TWO_HANDED` - Both hands on spear shaft

## Notes

- All phase/intensity parameters are automatically clamped to [0.0, 1.0]
- Lean parameters are automatically clamped to [-1.0, 1.0]
- IK solving automatically handles elbow and knee positions
- Body height is automatically adjusted when mounting
- All methods preserve the animation context for consistency
