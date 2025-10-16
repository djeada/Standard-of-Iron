# Knight Troop Type

## Overview
The **Knight** is a melee-focused heavy infantry unit designed for close-quarters combat. Knights serve as the primary frontline combatants, specializing in hand-to-hand combat and providing a robust presence on the battlefield.

## Unit Characteristics

### Combat Role
- **Type:** Melee Specialist
- **Combat Mode:** Melee only (no ranged capability)
- **Primary Function:** Close-range engagements and frontline assault

### Statistics
- **Health:** 150 HP (higher than Archers: 80 HP)
- **Max Health:** 150 HP
- **Speed:** 2.0 units/sec (slower than Archers: 3.0 units/sec)
- **Vision Range:** 14.0 units
- **Melee Range:** 1.5 units
- **Melee Damage:** 20 HP per hit
- **Melee Cooldown:** 0.6 seconds

### Equipment
- **Primary Weapon:** Sword (melee)
- **Secondary Equipment:** Shield (defensive)
- **Visual Indicator:** ⚔️ icon in UI

### Formation
- **Individuals per Unit:** 15 soldiers
- **Max Units per Row:** 5
- **Selection Ring Size:** 1.4 (larger than Archers)

## Behavior

### Combat Behavior
1. **Melee-Only Combat:** Knights cannot engage in ranged combat
2. **Target Priority:** Engages nearest enemy within melee range
3. **Pursuit:** Will chase targets that move out of melee range
4. **Height Restriction:** Maximum height difference of 2.0 units for combat

### Tactical Considerations
- **Strengths:**
  - High health pool makes Knights excellent at absorbing damage
  - Strong melee damage output for close combat
  - Effective as frontline units to protect Archers
  
- **Weaknesses:**
  - Slower movement speed makes them less mobile
  - Vulnerable to kiting tactics
  - No ranged capability means they must close distance to attack

## Production

### Training
- **Location:** Barracks
- **Cost:** 15 villagers (individuals per unit)
- **Build Time:** 10 seconds (default barracks production time)
- **Requirements:** Available barracks with production capacity

### Integration
Knights can be recruited from any barracks alongside Archers. Select a barracks and click the Knight button (⚔️) in the production panel.

## Technical Implementation

### Files Modified/Created
- `game/units/knight.h` - Knight class header
- `game/units/knight.cpp` - Knight implementation
- `game/units/factory.cpp` - Knight factory registration
- `game/units/troop_config.h` - Knight troop configuration
- `ui/qml/ProductionPanel.qml` - Knight recruitment UI
- `ui/theme.cpp` - Knight icon mapping
- `assets/visuals/unit_visuals.json` - Knight visual configuration
- `game/CMakeLists.txt` - Knight build integration

### Code Structure
```cpp
// Knight inherits from Unit base class
class Knight : public Unit {
public:
  static std::unique_ptr<Knight> Create(Engine::Core::World &world,
                                        const SpawnParams &params);
private:
  Knight(Engine::Core::World &world);
  void init(const SpawnParams &params);
};
```

### Component Configuration
```cpp
// Attack Component - Melee Only
m_atk->meleeRange = 1.5f;
m_atk->meleeDamage = 20;
m_atk->meleeCooldown = 0.6f;
m_atk->preferredMode = CombatMode::Melee;
m_atk->currentMode = CombatMode::Melee;
m_atk->canRanged = false;
m_atk->canMelee = true;
```

## Testing

### Test Map
A dedicated test map is provided at `assets/maps/knight_test.json` featuring:
- Player 1 with 3 Knights and 3 Archers
- Player 2 with 3 Knights and 3 Archers
- Central hill terrain for tactical positioning
- Victory condition: Eliminate enemy barracks

### Manual Testing Steps
1. Launch the game
2. Select "Knight Test Map" from map selection
3. Select your barracks
4. Click the Knight button (⚔️) to recruit additional Knights
5. Command Knights to engage enemy units
6. Observe melee combat behavior and damage output

### Validation Points
- ✅ Knights spawn correctly at map start
- ✅ Knights can be recruited from Barracks
- ✅ Knights display proper team colors
- ✅ Knights engage in melee combat only
- ✅ Knights have higher health than Archers
- ✅ Knights move slower than Archers
- ✅ Knight icon (⚔️) displays in UI and production queue

## Future Enhancements

### Potential Improvements
1. **Animation System:** Dedicated sword-swing and shield-block animations
2. **Visual Assets:** Custom 3D models for Knights with sword and shield
3. **Formation Tactics:** Shield wall or phalanx formations
4. **Upgrades:** Tech tree for improved armor or damage
5. **Special Abilities:** Charge attack or defensive stance
6. **Sound Effects:** Sword clashing and metal armor sounds

### Balancing Considerations
- Monitor Knight vs Archer combat effectiveness
- Adjust movement speed if Knights are too slow/fast
- Fine-tune damage output for fair gameplay
- Consider cost adjustments based on power level

## Integration with Existing Systems

### Compatible Systems
- ✅ Entity-Component-System (ECS)
- ✅ Combat System (melee combat)
- ✅ Movement System (pathfinding)
- ✅ Selection System (unit selection)
- ✅ Production System (barracks training)
- ✅ AI System (enemy AI can recruit Knights)
- ✅ Formation System (group movement)
- ✅ Victory Service (counts toward objectives)

### Visual Systems
- ✅ Team Colors (inherited from base Unit)
- ✅ Selection Rings (larger size: 1.4)
- ✅ Health Bars (displays 150 HP)
- ✅ Production Queue (shows ⚔️ icon)

## Conclusion

The Knight troop type successfully expands unit diversity and supports hand-to-hand combat gameplay. Knights provide players with a robust melee option that complements the existing ranged Archer units, enabling more diverse tactical strategies and army compositions.
