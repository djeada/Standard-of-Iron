# Healer Visual Animation Improvements

## Current System Analysis

### How Healers Currently Work
- **Healing Mechanism**: Uses the `HealingSystem` which spawns green arrow projectiles
- **Implementation**: 
  - `HealingSystem::process_healing()` iterates through healers
  - For each healer, it finds targets within `healing_range`
  - Spawns arrows via `arrow_system->spawnArrow()` with green color `(0.2, 1.0, 0.4)`
  - Arrows use standard projectile physics (arc trajectory)
- **Current Visual**: Static green arrows that look like damage projectiles but are green
- **Problem**: No visual distinction from combat; doesn't feel like healing at all

### Key Components
- **Healer Classes**: `Roman::HealerRenderer`, `Carthage::HealerRenderer`
- **Healing Component**: `Engine::Core::HealerComponent` with healing_range, healing_amount, healing_cooldown
- **Arrow System**: Generic `ArrowSystem` used for both damage and healing
- **Rendering**: Uses healer-specific shaders for visual customization

---

## Proposed Healing Visual Animations

### Option 1: **Healing Aura & Radiant Beam** (RECOMMENDED - Most Impressive)

#### Core Concept
Replace arrow projectiles with a **radiant healing beam** that flows from healer to target with an aura effect.

#### Visual Elements

1. **Healing Beam** (GPU Shader-based)
   - Flowing cylindrical beam with animated texture
   - Color: Bright golden-green with internal shimmer
   - Path: Curved arc from healer hand to target body
   - Animation: Flowing particles along the beam (like light flowing)
   - Effect: Beam pulses in sync with healing cooldown

2. **Healer Aura**
   - Expanding concentric rings around healer during healing
   - Color: Soft golden glow
   - Radius: Extends to healing_range
   - Opacity: Pulsing 0.3-0.6 based on active healing
   - Effect: Shows healing area of effect visually

3. **Target Healing Aura**
   - Bright glow around target being healed
   - Color: Golden-white with green tint
   - Animation: Spiral energy flowing upward
   - Duration: Brief burst when healing hits

4. **Particle Trails**
   - Small spheres traveling along beam path
   - Fade as they approach target
   - Create "droplets of healing energy" effect

#### Technical Implementation
- Create `healing_beam.vert/frag` shaders with animated texture coordinates
- New `HealingBeamProjectile` class (separate from arrows)
- GPU particle system for visual richness
- Animation time-based for smooth streaming effect

#### Advantages
- ✅ Instantly recognizable as healing (not damage)
- ✅ Visually impressive and satisfying
- ✅ Shows healing area of effect clearly
- ✅ Professional/polished feeling
- ✅ Can be disabled/shown based on settings
- ✅ Scales well for many simultaneous heals

---

### Option 2: **Ethereal Light Spiral** (Alternative - More Mystical)

#### Core Concept
Floating light spirals that materialize around target, ascending toward the sky.

#### Visual Elements

1. **Healer Cast Animation**
   - Hands glow with healing energy
   - Aura burst outward in a sphere
   - Duration: ~0.5 seconds

2. **Healing Spirals**
   - 3-4 spiral geometry elements per heal
   - Color: Iridescent green→gold gradient
   - Path: Spiral helix from healer to target center
   - Animation: Twist + upward motion simultaneously
   - Lifetime: 1-1.5 seconds

3. **Target Reception**
   - Golden light descends into target
   - Body gains soft golden glow
   - Health bar flashes green

#### Advantages
- ✅ Very distinct and magical
- ✅ Easier particle effect (no beam geometry)
- ✅ Fits fantasy aesthetic well

#### Disadvantages
- ❌ Less "professional" looking
- ❌ Harder to see when many units are healed
- ❌ May feel too slow/non-responsive

---

### Option 3: **Cross-Channeled Energy Field** (Most Technical)

#### Core Concept
Healer creates a zone effect with energy lines connecting to all healable targets.

#### Visual Elements

1. **Healing Zone**
   - Semi-transparent dome/sphere around healer
   - Animated wireframe pattern flowing inward
   - Color: Soft green with golden highlights

2. **Energy Connections**
   - Lines connecting healer to each nearby target
   - Pulsing brightness based on healing active/cooldown
   - Fading in/out smoothly

3. **Target Impact**
   - Energy node at target location
   - Glow pulses when healing is applied
   - Healing bar flashes simultaneously

#### Advantages
- ✅ Shows range and all targets at once
- ✅ Very technical/cool appearance
- ✅ Clear visual feedback on who is being healed

#### Disadvantages
- ❌ Complex to implement (line rendering)
- ❌ Can become cluttered with many healers
- ❌ More GPU cost

---

### Option 4: **Hybrid: Beam + Particle Aura** (Balanced)

#### Core Concept
Combines beam from Option 1 with particle effects for maximum impact.

#### Visual Elements

1. **Primary Beam** (like Option 1)
   - Solid energy beam from healer to target
   - Animated flowing texture

2. **Secondary Particle System**
   - Small healing droplets orbit the beam
   - Emit particles on impact at target
   - Create "healing energy splash" effect at destination

3. **Healer Visual Enhancement**
   - Hands glow with healing power
   - Body has subtle aura
   - Animation: Prayer/meditation pose during active healing

#### Advantages
- ✅ Combines best of multiple approaches
- ✅ Maximum visual feedback
- ✅ Multiple layers add depth
- ✅ Can be partially disabled if performance needed

---

## Implementation Strategy for Option 1 (Recommended)

### Phase 1: Core System
```
1. Create HealingBeamProjectile class (game/systems/)
   - Similar to StoneProjectile but for beams
   - Track start/end positions, progress, duration
   - No damage application (healing already handled in HealingSystem)

2. Modify HealingSystem
   - Replace arrow_system->spawnArrow() calls
   - Call healing_beam_system->spawnBeam() instead
   - Pass healer position, target position, healing duration

3. Create HealingBeamRenderer (render/geom/)
   - Render ProjectileBeam meshes
   - Apply healing_beam.vert/frag shaders
   - Animate texture coordinates based on time

4. Create Shaders (assets/shaders/)
   - healing_beam.vert: Curved path interpolation
   - healing_beam.frag: Golden-green color with inner glow
```

### Phase 2: Visual Enhancement
```
1. Create HealingAuraRenderer
   - Render expanding rings around active healer
   - Pulsing based on healing cooldown
   - CPU-based circle generation or GPU instanced rendering

2. Create ParticleTrail
   - Small sphere meshes following beam path
   - Fade opacity over lifetime
   - Staggered spawn for flowing effect

3. Target Impact Effect
   - Brief bright glow at target location
   - Ascending spiral particles
   - Synchronized with health bar visual feedback
```

### Phase 3: Audio-Visual Sync
```
1. Add healing sound effects
   - Healer cast sound (whoosh)
   - Beam travel sound (soft hum)
   - Impact sound at target (sparkle/ding)

2. Synchronize animations
   - Sound plays when beam starts
   - Beam speed affects sound duration
```

---

## Technical Considerations

### Performance
- **GPU Cost**: Shaders + particles, minimal compared to healer rendering
- **CPU Cost**: Beam system parallel to arrow system, negligible
- **Memory**: New mesh types (beams) but reusable across heals
- **Optimization**: Could disable in large-scale battles if needed

### Compatibility
- Works with existing healer system without breaking changes
- Arrow system remains for damage projectiles
- Can coexist during transition period

### Quality Levels
- **Ultra**: Full beam + aura + particles + impact effects
- **High**: Beam + particles only
- **Medium**: Beam only (no aura/particles)
- **Low**: Simplified beam or reverted to arrows

---

## Animation Timing Reference

### Heal Application
```
Time 0.0s:   Healer enters casting state
Time 0.1s:   Beam spawns, starts flowing
Time 0.2s:   Target receives healing (aura glow)
Time 0.8s:   Beam fades out
Time 1.0s:   Animation complete
Time 1.5s:   Next heal can occur (cooldown)
```

### Aura Pulsing
```
Breathing pattern: 1.5-2 second cycle
- 0.0-0.5s: Expand from healer center
- 0.5-1.0s: Maintain radius
- 1.0-1.5s: Pulse brighter
- 1.5-2.0s: Fade slightly, repeat
```

---

## Shader Pseudocode (healing_beam.vert)

```glsl
// Beam travels from point A to point B along curved arc
vec3 beam_position(float t, vec3 start, vec3 end) {
    // Linear interpolation with vertical arc
    vec3 mid = (start + end) * 0.5;
    mid.y += 0.3; // Arc height
    
    vec3 pos = mix(mix(start, mid, t), mix(mid, end, t), t);
    
    // Add spiral twist
    float twist = t * 6.28; // Full rotation
    float radius = 0.15 * (1.0 - abs(t - 0.5) * 2.0); // Barrel shape
    pos.x += cos(twist) * radius;
    pos.z += sin(twist) * radius;
    
    return pos;
}

// Fragment: Golden-green gradient with glow
vec4 healing_color(vec3 normal, float edge_dist) {
    float core = exp(-edge_dist * 10.0); // Center is bright
    
    // Golden-green gradient
    vec3 color = mix(vec3(0.2, 1.0, 0.4), vec3(1.0, 0.8, 0.2), core);
    
    // Add glow based on normal
    float glow = pow(1.0 - abs(normal.z), 2.0) * 0.5;
    color += glow;
    
    return vec4(color, core);
}
```

---

## Recommendation

**Go with Option 1: Healing Aura & Radiant Beam**

### Reasons
1. **Most Impressive**: Instantly elevates healer feedback
2. **Professional**: Looks polished and intentional
3. **Performant**: GPU-based, scales well
4. **Clear Feedback**: Unambiguous what's happening
5. **Sustainable**: Can be iteratively improved
6. **Reusable**: Beam system useful for other effects (buffs, debuffs, etc.)

### Quick Win Path
Start with a simple **curved beam** (no particles), add aura next, then particles/impact effects as polish passes.

---

## Questions for Design

1. Should heals of all units be visible or only friendly/player-visible ones?
2. Color preference: Golden-green, pure green, or faction-specific?
3. Should beam speed vary by healing amount?
4. Should multiple heals show multiple beams or combine into one?
5. Should impact zone be visible (where healing lands)?
