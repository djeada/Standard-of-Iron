# Rockfall Ambush

Defenders holding high ground can roll boulders onto a column in a pass below. This is how the Allobroges struck Hannibal's army on the climb into the Alps, and the Alps episode of _The Barcid Road_ is built on it.

A rockfall is a **trap**: a cache of boulders staged on the heights, aimed at a kill zone in the pass. Each trap is released once by a trigger. It can also rearm after a delay.

## Authoring a trap

Traps live in the map file, beside `undead_zones`:

```json
"rockfall_traps": [
    {
        "id": "allobroges_heights",
        "release": [118, 64],
        "target": {"x": 124, "z": 80},
        "radius": 8,
        "trigger": "ai",
        "owner_id": 3,
        "boulders": 6,
        "boulder_radius": 0.55,
        "spread": 6,
        "interval": 0.4,
        "damage": 40,
        "casualty_fraction": 0.22,
        "rearm": 0,
        "ai_min_targets": 2
    }
]
```

| Field                         | Default        | Meaning                                                                                                            |
| ----------------------------- | -------------- | ------------------------------------------------------------------------------------------------------------------ |
| `id`                          | `rockfall_<n>` | Name used by mission events and arena steps.                                                                       |
| `release`                     | required       | Where the boulders start, on the slope above the pass. Takes `[x, z]`, `[x, y, z]` or `{"x", "z"}`.                |
| `target`                      | required       | Centre of the kill zone. Boulders are pushed towards it.                                                           |
| `radius`                      | 8              | Kill-zone radius around `target`, used by the automatic triggers.                                                  |
| `trigger`                     | `zone`         | `zone`, `ai` or `scripted`; see below.                                                                             |
| `owner_id`                    | -1 (neutral)   | Who holds the heights. A trap never hurts its owner or their allies. A neutral trap counts every troop as hostile. |
| `boulders`                    | 5              | Boulders per release (1-24). They are fanned across `spread` metres and let go `interval` seconds apart.           |
| `boulder_radius`              | 0.55           | Boulder radius in world units. Each boulder is randomised by ±20%.                                                 |
| `damage`, `casualty_fraction` | 40, 0.22       | A strike deals the larger of `damage` and `casualty_fraction × max_health`, scaled by the boulder's speed.         |
| `rearm`                       | 0              | Seconds before a spent trap can fire again. 0 means single use.                                                    |
| `ai_min_targets`              | 2              | Number of hostile troops in the zone at which an AI defender releases at once.                                     |

Coordinates follow the map's `coord_system`, exactly like undead zones.

Choose a release point on the slope itself, not on a peak. Boulders follow the terrain gradient, so a boulder staged on the crest may roll down the far side. Each boulder also gets a push of 2.5-4 m/s towards `target` when it is let go.

## Triggers

- **`zone`**: fires as soon as a hostile troop is inside the kill zone.
- **`ai`**: the trap's owner has to be AI-controlled. The AI waits for the column: it releases once `ai_min_targets` hostile troops are in the zone, or after `k_rockfall_ai_patience_seconds` (3 s) with at least one there. Under a human owner the trap only fires by script.
- **`scripted`**: fires only from a mission event or an arena step.

Mission events release a trap by id with a `rockfall` action. See _Mission events_ in [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md). Arena scenarios use the `TriggerRockfall` step command with `zone_id` set to the trap id.

## Runtime behaviour

`Game::Systems::RockfallSystem` runs in the `Combat` phase. It is configured from the map by `configure_map_systems`.

Boulders are physics-lite spheres:

- **Rolling.** On the ground, gravity along the slope accelerates a boulder at 5/7 g, the rate for a rolling solid sphere. Rolling resistance slows it down. A bend from a steep wall into the valley floor redirects the boulder and keeps 85% of its speed, rather than stopping it.
- **Airborne.** When the ground falls away faster than the boulder can follow, it leaves the slope and flies ballistically. On landing it bounces with a restitution of 0.3, raises dust, and plays `combat.siege_impact` if the landing is hard.
- **Coming to rest.** Below 0.45 m/s on the ground the boulder settles. It stays as rubble for `k_rockfall_settled_linger_seconds` (8 s), sinks into the ground over the last 1.2 s, and is then removed.

A boulder moving faster than `k_rockfall_lethal_speed` strikes every hostile troop it passes within `boulder radius + k_rockfall_troop_reach`. It strikes each troop only once, and not when it is bounding over their heads. A strike:

- applies speed-scaled damage through `apply_unit_damage`. That kills soldiers in the formation, and its hit event sounds like a siege stone (`combat.hit.siege`);
- flings the fresh casualties along the boulder's path (`launch_new_casualties_along`);
- knocks the surviving troop down (`StaggerTier::Knockdown`); and
- costs the boulder 18% of its speed, so a column shields its rear ranks a little.

Releasing a trap plays the positioned `hazard.rockfall` cue. Dust puffs are drawn with the `stone_impact` effect, and boulders are drawn with the catapult stone mesh at boulder scale (`render_rockfall` in `render/geom/projectile_renderer.cpp`).

Trap state and boulders in flight are saved under the `rockfall` snapshot key.

## Arena

- `rockfall_alpine_pass` is a snowbound gorge. A three-troop column marches through it; a scripted trap on the northern heights fires when the column is halfway in, and a zone trap on the southern slope fires on its own.
- `rockfall_ai_defenders` has the same gorge, with the heights held by the AI owner.

Both expect the column to lose health, to show death animations, and to launch casualties.
