# Promo capture

Two programs make a promo video. The arena renders authored camera moves over a
scenario and writes one clip per shot; `scripts/promo-edit.py` joins, grades,
captions and scores those clips into a finished cut. Both read the **same spec
file**, so the camera work and the edit that presents it live next to each
other:

```sh
build/bin/arena_app --promo-spec tools/arena/promos/rome_iron_line.json \
  --promo-out artifacts/promo
scripts/promo-edit.py --spec tools/arena/promos/rome_iron_line.json \
  --clips artifacts/promo/rome_iron_line
```

`scripts/capture-formation-promos.sh` runs both steps over all three formation
reels; `--edit-only` re-cuts the footage already on disk without re-rendering
it, which is the loop to use while tuning captions, grade or transitions.

The arena needs a real display — it renders through OpenGL and `offscreen` has
no framebuffer. The edit needs `ffmpeg` on `PATH`.

## The spec

`tools/arena/promo_spec.h` defines what the arena reads; the arena ignores keys
it does not know, which is why the editorial fields (`title`, `grade`,
`caption`, `transition`, `music`) sit in the same file without the recorder
caring about them.

A shot names a scenario, a window into it (`start`, `duration`), what the camera
looks at (`focus`), and how it moves (`camera` keyframes). Each shot **reloads
and re-simulates its scenario from zero**, so shots may be authored in any order
and two shots may cover overlapping windows of the same battle from different
angles. `slow_motion` shrinks the simulation step rather than duplicating
frames, so a half-speed shot keeps full temporal detail.

Camera `yaw` blends along the shorter arc between keys: `8` followed by `352` is
sixteen degrees back, not a full orbit the other way. Author a middle key when
you really want the long way round.

## Transitions

`promo-edit.py` joins the clips. Each shot's `transition` describes the cut
*into* it, so the first shot's is ignored; the spec-level `transition` is the
default for every join.

```json
{
  "transition": {"type": "dissolve", "duration": 0.35},
  "shots": [
    {"name": "deploy", "transition": {"type": "dip", "duration": 0.5}}
  ]
}
```

`{"type": "cut"}` restores a hard cut for one join. The vocabulary is in
`TRANSITIONS` in the script — `dissolve`, `dip`, `flash`, `whip`, `smear`,
`push`, `zoom`, `radial`, `grain`, `iris`, `wipe`, `bleach`, `cut`. Runs of hard
cuts are concatenated and only the blended joins become `xfade` filters, because
`xfade` has no zero-length form.

Blended joins **overlap** their two shots, so the finished cut is shorter than
the sum of its clips. Caption timing is measured on the blended timeline and
held clear of the blend either side of it. `--transition` and
`--transition-duration` override the whole reel from the command line, which is
the quick way to compare an edit against hard cuts.

## Scenarios built for capture

Capture scenarios differ from acceptance scenarios in scale and dressing, not in
kind. `add_formation_promo_scenarios` in
`tools/arena/arena_formation_scenarios.cpp` holds the three formation reels;
`dress_for_capture` is the shared dressing pass.

Two settings matter more than they look:

- **`arena_floor_half_extent`** — the arena levels a flat square out of its
  mountain noise, 18 units either side by default. That is a duelling floor; an
  army deploying from column into line needs far more, and outside the square
  the ground is rough enough to break the shapes up. The floor eases into the
  surrounding noise over a taper rather than stepping out of it, so the
  mountains do not stand as a wall on the touchline.
- **the environment overrides** — leave `exposure_override` and
  `fog_density_override` alone unless the scene really needs them. Setting them
  is what turned the first generation of capture scenarios into murk. Locking
  `time_mode` is worth it though: a promo records the same scenario once per
  shot, and the light has to match across the cut. While you are still choosing
  the hour, `--time <hour>` overrides a locked scenario, so a lighting sweep is
  four batch captures rather than four rebuilds.

## Driving the formation system

`ScenarioCommandKind::FormArmy` is the step that shows the army formation layer
off. `FormationMove` only translates whatever shape a group already stands in;
`FormArmy` folds any number of groups into one army, asks the doctrine planner
where every unit belongs, writes the chosen slots back onto each unit's
formation mode so the runtime can measure cohesion against them, and walks
everyone there — the same path a player's formation drag takes.

```cpp
auto advance = form_step(36.0F, legion, Intent::Assault, {0, 0, 18}, 0.0F, 54.0F);
advance.formation.options.movement_policy = MovementPolicy::MaintainFormation;
```

Cycling one army through `Column`, `Line`, `Defensive` and `Assault` is what
makes a formation reel read as a system rather than as a battle.
