# Rockfall Ambush

Defenders holding high ground can roll boulders onto troops climbing towards them. This is how the Allobroges struck Hannibal's army on the climb into the Alps, and the Alps episode of _The Barcid Road_ is built on it.

## Stone caches on hill paths

Every hill ramp gets a **stone cache** when the map loads: a pile of boulders behind a timber crib, at the crest where the ramp meets the plateau. A ramp has to climb at least 1.5 m to get one, and ramps whose crests are within 5 m of each other share a cache. A map can turn this off with `"hill_rockfall_caches": false`.

- **Claiming.** A cache starts unclaimed and flies an undyed banner. The first side to get a troop within `k_rockfall_claim_radius` (5 m) of it claims it, and the standard claim banner turns to that side's colours. If the holder leaves and an enemy troop arrives, the enemy takes it over. A human player who claims a cache is told how to use it.
- **Rolling.** Select troops standing within `k_rockfall_use_radius` (7 m) of a cache your side holds, and **Roll Stones** appears in the action menu. Press it and the troop stops, turns downhill and heaves for `k_rockfall_push_seconds` (1.6 s), playing the crew-push work pose. The pile rocks and tips over the crib while they push. Then the boulders go over the edge one after another and down the ramp.
- **Effect.** A boulder crushes the couple of men in its lane in every troop it rolls through, at most 22% of a troop per strike, scaled by speed. A volley down a crowded ramp costs an assault dearly, but it no longer erases the army climbing it (it used to take half of every troop it touched). Dead soldiers are flung down the slope, and survivors are knocked down.
- **Once only.** A cache is used once. When its last boulder goes, it is gone.
- **Telegraph.** Your own caches mark the ramp they cover with chevrons in your colour. While enemies are on the ramp, the chevrons pulse red and you get the alert "Enemies are climbing below your stones".
- **AI.** An AI that holds a cache rolls it by itself, using its nearest troop. It waits until a climber is in the upper 70% of the ramp, where the stones will catch them.

## Authoring a trap

Maps can also stage their own caches and traps beside `undead_zones`. Authored entries use the same physics. They can be claimable like hill caches, or they can fire on their own.

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

| Field                         | Default        | Meaning                                                                                                                                                                                                                         |
| ----------------------------- | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `id`                          | `rockfall_<n>` | Name used by mission events and arena steps.                                                                                                                                                                                    |
| `release`                     | required       | Where the boulders start, on the slope above the pass. Takes `[x, z]`, `[x, y, z]` or `{"x", "z"}`.                                                                                                                             |
| `target`                      | required       | Centre of the kill zone. Boulders are pushed towards it.                                                                                                                                                                        |
| `radius`                      | 8              | Kill-zone radius around `target`, used by the automatic triggers.                                                                                                                                                               |
| `trigger`                     | `zone`         | `zone`, `ai` or `scripted`; see below.                                                                                                                                                                                          |
| `owner_id`                    | -1 (neutral)   | Who holds the heights. A trap never hurts its owner or their allies. A neutral trap counts every troop as hostile.                                                                                                              |
| `boulders`                    | 5              | Boulders per release (1-24). They are fanned across `spread` metres and let go `interval` seconds apart.                                                                                                                        |
| `boulder_radius`              | 0.55           | Boulder radius in world units. Each boulder is randomised by ±20%.                                                                                                                                                              |
| `damage`, `casualty_fraction` | 40, 0.22       | A strike crushes the men in the boulder's lane: about 2.5 soldiers' worth of the troop's health, never more than `casualty_fraction × max_health` and never less than `damage`, scaled by the boulder's speed (×0.45 to ×1.25). |
| `rearm`                       | 0              | Seconds before a spent trap can fire again. 0 means single use.                                                                                                                                                                 |
| `ai_min_targets`              | 2              | Number of hostile troops in the zone at which an AI defender releases at once.                                                                                                                                                  |

Coordinates follow the map's `coord_system`, exactly like undead zones.

Choose a release point on the slope itself, not on a peak. Boulders follow the terrain gradient, so a boulder staged on the crest may roll down the far side. Each boulder also gets a push of 2.5-4 m/s towards `target` when it is let go.

## Triggers

- **`claim`**: a stone cache like the hill ones. It is taken by presence and rolled by order, or automatically by an AI holder.
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

Releasing a trap plays the positioned `hazard.rockfall` cue. `render_rockfall` in `render/geom/projectile_renderer.cpp` draws:

- the caches, with their timber crib and the standard hanging claim banner (`BarracksFlagRenderer::draw_hanging_banner`);
- the ramp chevrons;
- the boulders, as the stone mesh squashed per boulder into lumpier, colour-varied granite; and
- the dust, with the `stone_impact` effect.

The **Roll Stones** order is the `RollStones` command payload. Behind it:

- the HUD action `roll_stones`, which appears only when a selected troop has a cache in reach;
- `OrdersReadout::stones_ready`, which the HUD polls without taking the frame lock; and
- `RockfallPushComponent`, which drives the push pose.

Trap state and boulders in flight are saved under the `rockfall` snapshot key.

## Arena

- `rockfall_hill_ramp` stages spearmen beside the cache on a mesa's ramp. They roll it onto a column of swordsmen climbing the ramp.
- `rockfall_hill_ai` gives the cache to the AI, which rolls it on its own when the player's column climbs.

- `rockfall_alpine_pass` is a snowbound gorge. A three-troop column marches through it; a scripted trap on the northern heights fires when the column is halfway in, and a zone trap on the southern slope fires on its own.
- `rockfall_ai_defenders` has the same gorge, with the heights held by the AI owner.

Both expect the column to lose health, to show death animations, and to launch casualties.
