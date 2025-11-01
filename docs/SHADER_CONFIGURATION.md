# Nation-Troop Shader Combinations

This document describes the unique render configuration and shaders for each nation-troop combination in the game.

## Overview

Each combination of troop type (Archer, Swordsman, Spearman, Horse Swordsman) and nation (Kingdom of Iron, Roman Republic, Carthage) has its own unique vertex and fragment shader pair. This ensures visual distinction and proper rendering of nation-specific equipment and armor.

## Shader Naming Convention

Shaders follow the pattern: `{troop_type}_{nation_id}.{vert|frag}`

Examples:
- `archer_kingdom_of_iron.vert` / `archer_kingdom_of_iron.frag`
- `swordsman_roman_republic.vert` / `swordsman_roman_republic.frag`
- `spearman_carthage.vert` / `spearman_carthage.frag`

## Nation-Specific Characteristics

### Kingdom of Iron

**Visual Theme:** Heavy medieval European armor with iron and steel

#### Archer
- **Helmet:** Great helm/coif design
- **Torso:** Padded gambeson with light mail
- **Lower:** Leather belt with cloth skirt
- **Colors:** Steel grey, dark cloth

#### Swordsman
- **Helmet:** Great helm (closed face)
- **Torso:** Heavy plate armor with chainmail
- **Lower:** Cuisses and greaves
- **Colors:** Polished steel, dark leather

#### Spearman
- **Helmet:** Kettle helm or bascinet
- **Torso:** Brigandine with mail hauberk
- **Lower:** Tassets and faulds
- **Colors:** Steel plates, reinforced cloth

#### Horse Swordsman
- **Helmet:** Great helm with aventail
- **Torso:** Full plate armor/coat of plates
- **Lower:** Leg armor with horse caparison
- **Colors:** Knight's plate, heraldic colors

---

### Roman Republic

**Visual Theme:** Classical Roman military equipment with brass and bronze accents

#### Archer
- **Helmet:** Auxiliary helmet design
- **Torso:** Lorica hamata (chainmail) or squamata (scale)
- **Lower:** Pteruges (leather strips) with cingulum belt
- **Colors:** Bronze/brass tones, red cloth accents

#### Swordsman
- **Helmet:** Galea (Roman legionary helmet)
- **Torso:** Lorica segmentata (banded armor)
- **Lower:** Pteruges with military belt
- **Colors:** Brass, red tunic, leather

#### Spearman
- **Helmet:** Montefortino helmet
- **Torso:** Pectorale (heart guard) or lorica hamata
- **Lower:** Military belt with pteruges
- **Colors:** Bronze, red accents

#### Horse Swordsman
- **Helmet:** Attic or Phrygian helmet
- **Torso:** Bronze muscle cuirass or lorica squamata
- **Lower:** Riding pteruges with horse blanket
- **Colors:** Polished bronze, red cavalry cloth

---

### Carthage

**Visual Theme:** Mediterranean mix of Greek, Phoenician, and local North African styles

#### Archer
- **Helmet:** Leather cap or light helmet
- **Torso:** Linothorax (linen armor)
- **Lower:** Leather skirt/pteruges
- **Colors:** Bronze with patina, light fabrics

#### Swordsman
- **Helmet:** Montefortino or Corinthian style
- **Torso:** Bronze cuirass or linothorax
- **Lower:** Bronze greaves with leather skirt
- **Colors:** Bronze with green patina, white/blue cloth

#### Spearman
- **Helmet:** Captured equipment or Iberian style
- **Torso:** Linothorax or captured mail
- **Lower:** Leather greaves/pteruges
- **Colors:** Mixed equipment, earth tones

#### Horse Swordsman (Numidian Cavalry)
- **Helmet:** Bronze cap or bare-headed
- **Torso:** Light tunic or imported mail
- **Lower:** Bare legs with simple saddle blanket
- **Colors:** Light cavalry equipment, natural tones

## Vertex Shader Features

All nation-specific vertex shaders include:

1. **Armor Layer Detection:** Uses Y-position to classify body parts into layers (0=helmet, 1=torso, 2=lower)
2. **Nation-Specific Comments:** Documents the specific armor types for that nation
3. **Unique Thresholds:** Different Y-position thresholds reflect different armor designs

## Fragment Shader Features

Nation-specific fragment shaders include:

1. **Material Textures:** Nation-appropriate metal, leather, and cloth patterns
2. **Color Palettes:** Culturally appropriate color schemes
3. **Detail Patterns:** Chainmail, scale, plate articulation specific to each culture
4. **Weathering:** Period and region-appropriate wear patterns

## Renderer Configuration

Each troop type has three nation-specific renderers:
- `render/entity/nations/kingdom/{troop}_renderer.cpp`
- `render/entity/nations/roman/{troop}_renderer.cpp`
- `render/entity/nations/carthage/{troop}_renderer.cpp`

Each renderer:
1. Resolves the appropriate shader based on the unit's nation
2. Falls back to the base shader if nation-specific shader is unavailable
3. Uses the style configuration to customize visual appearance

## Validation

Run the validation script to ensure all shaders are properly configured:

```bash
python3 scripts/validate_shader_config.py
```

Expected output: All 12 nation-troop combinations should show ✓ for vertex shader, fragment shader, and style configuration.

## Adding New Nations

To add a new nation:

1. Create vertex and fragment shaders for all 4 troop types:
   - `{troop}_{nation_id}.vert`
   - `{troop}_{nation_id}.frag`

2. Create style configuration files:
   - `render/entity/nations/{nation}/{troop}_style.cpp`

3. Create renderer files if needed (can often reuse base renderer)

4. Update this documentation with the new nation's characteristics

5. Run validation to confirm all files are properly configured
