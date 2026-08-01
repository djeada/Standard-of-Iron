# Formation content

Two independent layers live here. They are never mixed.

| Layer | Directory | Scale | Consumed by |
|---|---|---|---|
| Unit layout | `unit_layouts/` | soldiers inside **one** troop entity | `Game::Formation::UnitLayoutSystem`, renderer |
| Army formation | `army/` | placement of **many** troop entities | `Game::Formation::ArmyFormationPlanner`, player commands, AI |

Both are optional: every id shipped here also exists as a built-in default in
`game/formation/`. A file in this directory **overlays** the built-in of the same
`id`, so a partial file only has to list the fields it changes.

`content_validator` fails the build on unknown role tags, unknown layout ids
referenced by troops, doctrines without a `faction_default` template, and
non-positive spacing scales.

## `unit_layouts/*.json`

Layout ids are resolved as `"<doctrine>.<generic>"` first, then `"<generic>"`.
So `rome.close_order_infantry` overrides `close_order_infantry` for Roman
troops only, and any doctrine without its own variant falls back to the generic
one. Adding a faction means adding `<new_doctrine>.*` files — no code changes.

```json
{
  "id": "rome.close_order_infantry",
  "shape": "ranks",
  "lateral_spacing_scale": 1.10,
  "depth_spacing_scale": 1.12,
  "rank_stagger": 0.15,
  "front_rank_tightening": 0.10,
  "lateral_jitter": 0.035,
  "facing_jitter_degrees": 1.8,
  "min_separation_scale": 0.62
}
```

`shape` is one of `ranks`, `wedge`, `loose_order`, `column`, `cluster`,
`circle`, `shell`, `arc`. Jitter is deterministic: it is a hash of the unit
seed and the soldier index, and it is clamped so two soldiers can never come
closer than `min_separation_scale` of the nominal spacing.

## `army/*.json`

```json
{
  "id": "rome",
  "display_name": "Roman Republic",
  "default_intent": "line",
  "intents": {
    "faction_default": {
      "frontage_scale": 1.0,
      "reserve_rows": 1,
      "lines": [
        {
          "role": "screen",
          "match_any": ["spear_infantry"],
          "exclude": ["cavalry"],
          "placement": "centre_block",
          "max_per_row": 8
        },
        { "role": "reserve", "max_per_row": 6 }
      ]
    }
  }
}
```

Lines are matched in order; the first rule whose `match_any` / `match_all` /
`exclude` masks accept a troop's role tags claims it. A rule with no masks is a
catch-all and must come last — without one, troops with unusual roles would be
appended to whatever the final rule is.

Templates never name concrete troop ids. They select on the role tags a troop
declares in its own `formation` block:

```json
"formation": {
  "individuals_per_unit": 15,
  "max_units_per_row": 5,
  "roles": ["line_infantry", "shielded"],
  "unit_layout": "close_order_infantry",
  "defensive_layout": "shield_wall",
  "army_roles": ["centre", "reserve"]
}
```

Intents are `faction_default`, `line`, `column`, `defensive`, `assault`,
`encirclement`, `siege_escort`. An intent may declare `requires_roles` plus a
`requirement_hint`; the UI shows that hint as the reason the preset is greyed
out for the current selection.
