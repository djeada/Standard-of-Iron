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
_into_ it, so the first shot's is ignored; the spec-level `transition` is the
default for every join.

```json
{
    "transition": { "type": "dissolve", "duration": 0.35 },
    "shots": [
        { "name": "deploy", "transition": { "type": "dip", "duration": 0.5 } }
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
- **the horizon** — the grey ring on the skyline is not the arena terrain and
  no floor extent or fog density will move it. It is `MapBoundaryFogRenderer`,
  drawn from the height map's own dimensions, and `arena_floor_half_extent`
  clamps to the 96x96 grid (about 45 either way) however large a number it is
  given. `suppress_boundary_mountains` configures that ring away — but the ring
  is also what hides the cut edge of the field, so a scene that suppresses it
  has to close its own horizon. `elevation_patches` laid in an overlapping ring
  does that in the scene's own grass; make the mounds tall enough that the
  widest shot's camera still sits below their tops.
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

## Which frames end up in a clip

Only frames the capture driver asked for are recorded. That is not a detail: in
batch and promo mode the arena drives its own paints from a timer
(`makeCurrent(); paintGL(); doneCurrent();`), but the window system still asks
for paints of its own on exposure, resize and damage. Those paints run `paintGL`
with the simulation step suppressed, so before this was fixed an unscheduled
repaint could push a duplicate frame into the encoder and shorten the shot by
one authored frame at the other end. Capture now requires the frame to be a
sampled one, and the shot state machine only ticks on sampled frames, so a
window event can never enter the recording.

A shot also waits before it records:

- **Pass warm-up.** A freshly loaded scenario has not had its terrain, props and
  creature meshes through a complete render yet. `k_pass_warmup_frames` frames
  of the loaded scenario go by before any shot in that pass may record, which is
  what keeps a shot that opens at scenario time zero from starting on a
  half-built frame. Before this, such a shot logged `soldiers 0/0 drawn` on its
  first frame.
- **Step arming.** The frame that switches the simulation to the shot's own step
  (`slow_motion` shrinks it) was itself simulated with the idle step. That one
  frame is dropped rather than recorded.

If you are hunting a bad opening frame, measure rather than eyeball it:

```sh
ffmpeg -v info -i 01_shot.mp4 \
  -vf "select='lt(n,4)',signalstats,metadata=print:key=lavfi.signalstats.YAVG" \
  -fps_mode passthrough -f null - 2>&1 | grep YAVG
```

## The captured frame is stamped opaque

Every blended pass dents the framebuffer's alpha where it covered a pixel. The
scatter shaders write their coverage as source alpha, so a leaf silhouette ends
the frame at an alpha of `1/255`. Nothing on screen reads that channel, which is
why the game looks right — but a capture does: the FBO comes back from `QImage`
labelled premultiplied, and the conversion on the way to the encoder divides
colour by alpha, which turned every plant edge into a white fringe that existed
only in recorded footage.

`ArenaViewport::stamp_capture_alpha_opaque` clears the alpha channel back to
opaque behind a colour mask, immediately before the readback, so colour is
untouched. If a new translucent pass ever shows a bright outline in captures and
not in the game, check the alpha channel of the poster PNG first — the fringe
pixels will be the non-opaque ones, and it is not a shading bug.

Note that the _finished cut_ opens on black by design — `promo-edit.py` applies
an `OPENING_FADE` from black, so a dark first frame there is the edit, not the
recorder.

## The commander duel reel

`commander_duel.json` over `promo_commander_duel` is the single-combat reel:
both armies halt in line, Scipio and Hannibal walk out, and the fight runs until
Hannibal goes down. Four things about it generalise to any close shot of a
fight:

- **Frame the pair side-on, and compute the angle rather than guessing.** The
  camera offset is `(sin(yaw), _, cos(yaw))`, so a side-on lens sits
  perpendicular to the line between the two fighters:
  `yaw = atan2(-(b.z - a.z), b.x - a.x)`. Duellists circle each other, so that
  angle moves through the fight — read the pair's positions out of `trace.jsonl`
  at each shot's window and set the keys from them. A yaw that happens to lie
  along the pair puts one man completely behind the other, which is what a
  "why is the hero's back to me" shot always turns out to be.
- **Stay on the lit arc.** With this scenario's hour the field reads well from
  roughly `yaw 300` through `140`; the opposite side is in the ring's shadow and
  comes back muddy. Keeping every shot on one side also keeps the cut on one
  side of the line.
- **Cut to the trace, not to the clock.** Signature moves are the beats worth
  slow-motion, and they are the frames where a commander's `combat_action_id` is
  `RtsCommander*`. The killing blow is the frame the loser's health reaches
  zero — several seconds before the body is removed, so a shot authored off the
  removal time opens on a corpse.
- **Distance 7–9 m is the usable close range.** Nearer than about 6 m the troop
  meshes stop holding up and the fighters overlap; further than about 10 m the
  duel stops being the subject.

```sh
build/bin/arena_app --promo-spec tools/arena/promos/commander_duel.json \
  --promo-out artifacts/promo
scripts/promo-edit.py --spec tools/arena/promos/commander_duel.json \
  --clips artifacts/promo/commander_duel
```

## The humanoid showcase reel

`humanoid_showcase.json` over the `promo_humanoid_showcase` scenario is the
character reel: walk, run, leap, front flip, side aerial, handstand, sword
flourish and a spear thrown at a statue. It is staged differently from the
formation reels and the differences are deliberate:

- **Three performers, not one.** Equipment is resolved per renderer key and
  baked into the archetype, so a single actor cannot put a sword down and pick a
  spear up. The reel uses `showcase_athlete` (bare), `showcase_blademaster`
  (sword) and `showcase_lancer` (spear), all helmetless with greaves and a light
  cuirass, plus a fourth actor who only walks and runs.
- **`render_scale_override` is 1.0.** Troops render at 0.5-0.6 scale, which
  reads at the RTS camera but makes a character close-up look like a doll beside
  grass tufts authored for the world scale. The showcase actors render at full
  human height so props, scatter and shadows sit at the right size around them.
- **Seed 44.** The arena scatters iron ore procedurally, and an ore mound
  renders with an emissive purple vein shader that pulls the eye off the
  performer. Seed 44 leaves the stage clear; check any new seed before using it.
- **The acrobat drifts.** A front flip and a side aerial carry real root motion,
  so the routine walks the performer roughly 2.6 m left and 1.6 m forward per
  loop. Shots are authored inside the first loop (scenario time 1-13 s) where
  his position is known.
