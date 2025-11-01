# HumanoidPoseController Usage Examples

This document demonstrates how to use the new `HumanoidPoseController` API instead of directly manipulating pose coordinates.

## Migration Guide

### Before: Low-level Pose Manipulation

```cpp
// Old approach: Direct coordinate manipulation
void customizePose(const DrawContext &ctx,
                   const HumanoidAnimationContext &anim_ctx,
                   uint32_t seed, HumanoidPose &pose) const override {
  using HP = HumanProportions;
  
  // Manual kneeling logic
  if (anim_ctx.inputs.isInHoldMode) {
    float const kneel_depth = 0.45F;
    float const pelvis_y = HP::WAIST_Y - kneel_depth;
    pose.pelvisPos.setY(pelvis_y);
    
    float const stance_narrow = 0.12F;
    pose.knee_l = QVector3D(-stance_narrow, HP::GROUND_Y + 0.08F, -0.05F);
    pose.footL = QVector3D(-stance_narrow - 0.03F, HP::GROUND_Y, -0.50F);
    // ... more manual coordinate setting
    
    // Manual elbow IK
    QVector3D right_axis = pose.shoulderR - pose.shoulderL;
    right_axis.setY(0.0F);
    right_axis.normalize();
    QVector3D const outward_l = -right_axis;
    pose.elbowL = elbowBendTorso(pose.shoulderL, pose.handL, outward_l, 
                                 0.45F, 0.15F, -0.08F, +1.0F);
  }
}
```

### After: High-level Pose Controller API

```cpp
// New approach: High-level pose controller
void customizePose(const DrawContext &ctx,
                   const HumanoidAnimationContext &anim_ctx,
                   uint32_t seed, HumanoidPose &pose) const override {
  HumanoidPoseController controller(pose, anim_ctx);
  
  // Simple, readable kneeling
  if (anim_ctx.inputs.isInHoldMode) {
    controller.kneel(1.0F); // Full kneel
  }
}
```

## Common Use Cases

### 1. Kneeling Archer

```cpp
void customizePose(const DrawContext &ctx,
                   const HumanoidAnimationContext &anim_ctx,
                   uint32_t seed, HumanoidPose &pose) const override {
  HumanoidPoseController controller(pose, anim_ctx);
  
  const AnimationInputs &anim = anim_ctx.inputs;
  
  if (anim.isInHoldMode) {
    // Kneel with interpolation
    float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);
    controller.kneel(t);
    
    // Position hands for bow drawing
    controller.placeHandAt(true, QVector3D(-0.15F, pose.shoulderL.y() + 0.30F, 0.55F));
    controller.placeHandAt(false, QVector3D(0.12F, pose.shoulderR.y() + 0.15F, 0.10F));
  }
}
```

### 2. Leaning Spearman

```cpp
void customizePose(const DrawContext &ctx,
                   const HumanoidAnimationContext &anim_ctx,
                   uint32_t seed, HumanoidPose &pose) const override {
  HumanoidPoseController controller(pose, anim_ctx);
  
  const AnimationInputs &anim = anim_ctx.inputs;
  
  if (anim.is_attacking && anim.isMelee) {
    // Lean forward during thrust
    float const attack_phase = std::fmod(anim.time * 2.5F, 1.0F);
    if (attack_phase >= 0.30F && attack_phase < 0.50F) {
      float const t = (attack_phase - 0.30F) / 0.20F;
      controller.lean(QVector3D(0.0F, 0.0F, 1.0F), t * 0.6F);
    }
  }
}
```

### 3. Melee Strike with Hand Placement

```cpp
void customizePose(const DrawContext &ctx,
                   const HumanoidAnimationContext &anim_ctx,
                   uint32_t seed, HumanoidPose &pose) const override {
  using HP = HumanProportions;
  HumanoidPoseController controller(pose, anim_ctx);
  
  const AnimationInputs &anim = anim_ctx.inputs;
  
  if (anim.is_attacking && anim.isMelee) {
    float const attack_phase = std::fmod(anim.time * 2.0F, 1.0F);
    
    if (attack_phase < 0.25F) {
      // Wind up
      float const t = attack_phase / 0.25F;
      QVector3D const raised_pos(0.30F, HP::HEAD_TOP_Y + 0.2F, -0.05F);
      controller.placeHandAt(false, raised_pos);
    } else if (attack_phase < 0.55F) {
      // Strike
      float const t = (attack_phase - 0.25F) / 0.30F;
      QVector3D const strike_pos(0.35F, HP::WAIST_Y, 0.45F);
      controller.placeHandAt(false, strike_pos);
    }
  }
}
```

## Benefits

1. **Readability**: High-level intent is clear (e.g., `kneel()` vs manual coordinate manipulation)
2. **Maintainability**: IK logic is centralized, reducing code duplication
3. **Consistency**: All units use the same kneeling/leaning behavior
4. **Safety**: Bounds checking and validation built into the controller
5. **Testability**: Pose manipulation logic can be unit tested independently

## Backward Compatibility

The new controller doesn't break any existing code. You can:
- Continue using low-level coordinate manipulation if needed
- Mix controller usage with manual adjustments
- Gradually migrate renderers to use the new API

The controller internally uses the same IK functions (`elbowBendTorso`, etc.) that existing code uses, ensuring identical behavior.

## Advanced Usage

### Custom IK Parameters

```cpp
HumanoidPoseController controller(pose, anim_ctx);

// Use low-level IK with custom parameters
QVector3D const shoulder = pose.shoulderR;
QVector3D const hand(0.35F, 1.15F, 0.75F);
QVector3D const outward_dir(1.0F, 0.0F, 0.0F);

// Custom elbow bend
QVector3D const elbow = controller.solveElbowIK(
  false,           // Right arm
  shoulder, hand,
  outward_dir,
  0.50F,          // Custom along fraction
  0.20F,          // Custom lateral offset
  -0.05F,         // Custom Y bias
  1.0F            // Outward sign
);

pose.elbowR = elbow;
```

### Combining Multiple Operations

```cpp
HumanoidPoseController controller(pose, anim_ctx);

// Complex pose combining multiple operations
controller.kneel(0.5F);
controller.lean(QVector3D(0.0F, 0.0F, 1.0F), 0.3F);
controller.placeHandAt(true, QVector3D(-0.20F, pose.shoulderL.y(), 0.60F));
controller.placeHandAt(false, QVector3D(0.25F, pose.shoulderR.y(), 0.40F));
```
