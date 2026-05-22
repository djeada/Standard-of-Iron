# Audio coverage and wishlist

## Current shipped coverage

- `assets/audio` currently contains **101** audio files.
- `assets/audio/audio_manifest.json` now exposes **94** of them to the runtime.
- The remaining **9** packaged-but-unmanifested files are:
  - `sfx/combat/arrows_fast_flybys_lr.ogg`
  - `sfx/combat/arrows_many_overhead.ogg`
  - `sfx/combat/arrows_overhead_ambience.ogg`
  - `sfx/combat/arrows_overhead_dark.ogg`
  - `voices/carthage/baal_cultist.ogg`
  - `voices/carthage/infantry.ogg`
  - `voices/carthage/numidians.ogg`
  - `voices/roman/fire_legionary.ogg`
  - `voices/roman/praetorian_guard.ogg`

## What is routed now

- Frontend music now rotates across all matching menu and campaign tracks instead of pinning to the primary variant.
- Mission ambient-state music now keeps pools for peaceful, tense, combat, victory, and defeat, so faction-specific and alternate combat/result tracks can play.
- Mission ambience is now selected from manifest tags plus mission/map context instead of a single hardcoded terrain-to-track mapping.
- Combat SFX now use small per-family pools for melee, spear, arrow, cavalry, elephant, generic impact, death, victory, and defeat transitions while keeping shared cooldown buckets.
- Combat-state stingers now use faction-specific Roman/Carthaginian horn tracks when the local player enters combat.
- Building-under-attack and barrack-capture alerts are now owner-aware, so the player only hears threat/capture warnings for their own side; captured barracks can also use the marching-mass reinforcement variant.

## Wishes for more tracks

1. **Dedicated UI sounds**: hover/click still reuse `low_resources_click`; a real UI hover, confirm, cancel, and tab-switch set would remove the current placeholder feel.
2. **Weather-specific ambience**: fog, snowstorm, and winter-camp ambience would fit Trasimene/Trebia/Alps better than reusing generic mountain or battlefield beds.
3. **Large ranged-battle whooshes**: the remaining arrow flyby/overhead assets still need a dedicated volley-scale or projectile-passing event so they do not replace ordinary hit sounds.
4. **Elite/specialized unit voices**: if the unit roster expands to use them, wire the remaining `praetorian_guard`, `fire_legionary`, `baal_cultist`, `numidians`, and generic `infantry` clips through manifest tags and unit mappings.
