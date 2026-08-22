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

## Frame zero is the thumbnail

Every social platform takes the first frame of an upload as the poster image, so
a black frame zero is a black thumbnail. This has shipped three times, each time
from a different layer, so the rule is enforced in three places rather than
argued about:

- **Nothing in the edit may fade up from black.** `OPENING_FADE` is 0, and a
  card sitting at timeline zero passes `fade=0.0` to `drawtext` so its text is
  at full opacity on the very first frame. This is the one that bit last: the
  advisory card paints an opaque black box over the whole frame for its hold and
  then faded its text in over the first quarter-second, which is sixteen pure
  black frames at 60 fps. Nobody sees that in review — the card looks right the
  moment you scrub anywhere into it.
- **The check counts visible pixels, not the brightest one.** Peak luma alone
  passes anything with a single hot sample in it, and film grain over a black
  card supplies one; a mean would reject a legitimate title card, which is 99%
  black on purpose. `FIRST_FRAME_MIN_VISIBLE` is the fraction of the frame at or
  above `FIRST_FRAME_VISIBLE_LUMA`, which separates "an image" from "black with
  something in it". The shipped advisory card measures about 0.95%.
- **A refused cut is never left where it publishes.** `promo-edit.py` encodes to
  `<name>.staging.mp4` and only renames it over `<name>.mp4` once every delivery
  check has passed; a failure moves it to `<name>.rejected.mp4` and deletes any
  earlier `<name>.mp4`. Reporting a non-zero exit status is not enough on its
  own — a reel is cut at the end of a long pipeline, the message scrolls past,
  and a plausible `.mp4` under the expected name is what gets uploaded.

`tests/promo_first_frame_test.sh` runs the shipped trailer spec through the real
script on stand-in footage and asserts all of it, including that a black opening
is refused _and_ leaves nothing uploadable behind. It is in the PR gate, needs no
compiler, and takes about 25 seconds.

To measure a finished cut by hand:

```sh
ffmpeg -v info -i trailer.mp4 \
  -vf "select='lt(n,20)',signalstats,metadata=print:key=lavfi.signalstats.YMAX" \
  -fps_mode passthrough -f null - 2>&1 | grep YMAX
```

A run of `YMAX=16` frames at the head is a black opening — 16 is the limited-
range floor, not a dim image.

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

## The wolf attack reel

`wolf_attack.json` over `promo_wolf_attack` is the village reel: a pack comes out of the
east at villagers working the ground, catches the straggler, and the mounted watch rides
down the street to break it. Four things about it generalise to any reel shot over
gameplay systems rather than over a scripted battle:

- **The scene has to be true before the camera can help.** The first cut of this reel was
  eight wolves standing in one another inside a ring smaller than a wolf, on villagers who
  strolled back into the pack after being bitten. No camera move rescues that. The pack
  ring (`close_and_bite`), the civilian flight (`endangers_residents`) and the civilian
  retaliation exclusion (`can_retaliate`) were all fixed for this reel and all of them are
  gameplay fixes, not capture ones.
- **Stage the fight where the scenery is.** The villagers used to work 25 m east of their
  own village, so every close shot was a green field. Moving their work area next to the
  street put houses, carts and the temple in the background of half the reel, and the
  fleeing villagers now run through their own settlement.
- **Cut to the trace, captured at the reel's own frame rate.** `--batch` the scenario and
  read `trace.jsonl` for the frames that matter — the first bite, the health that reaches
  zero, the frame a rider's `melee_lock` goes true — then author `start` from those
  numbers. **The simulation is not frame-rate independent**, so a trace taken at
  `--fps 30` does not describe a reel the arena renders at 60. Measured on this scenario,
  the same seed produced 28 bites and a dead villager at 30 fps and 20 bites and no death
  at all at 60. Every shot was aimed at events that never happened in the footage, which
  is why the first cut showed wolves attacking and nobody dying. Pass the spec's own
  `fps` to the batch run.
- **Wildlife is in the trace too.** Each frame carries an `animals` array — id, species,
  position, health, behaviour, `focus_id`, `biting`, `dying` — because wolves belong to no
  scenario group and were invisible to the `units` array. Without it there is no way to
  aim a camera at a pack or to know when one dies, and the shots get authored on guesses.
- **Then re-place the audio.** `scripts/place-promo-cues.py` reads the same trace and
  rewrites the spec's `sfx` list onto the finished timeline. Run it after every retime.

`suppress_combat_dust` used to take the blood with it. Blood stains were drawn inside
`render_combat_dust`, so a scene that turned the dust dome off also lost every stain, and
a villager could be bitten seventeen times without marking the grass. `render_blood_stains`
is its own entry point now and always runs. (Stains themselves only ever spawned when a
body dropped — a lone civilian being torn apart produced none until the wolf bite path
started asking for them directly.)

A pack's `alert_radius` decides whether the rescue is a clash or a chase. At 18 the wolves
read five approaching riders as overwhelming while they were still four seconds out, broke
before contact, and were cut down one at a time over sixteen seconds and twenty-five
metres — on screen the riders arrive, nothing happens, and later the wolves are simply
dead. At 9 the pack holds its kill until the horses are on top of it, and three wolves die
within two metres of the body inside two seconds. Stage the radius against the distance
the rescue covers, not against realism.

**Flash transitions are a photosensitivity hazard, not a punctuation mark.** A `flash`
join drives the whole frame to white in about five frames; measured on the finished cut,
the two flashes in this reel produced single-frame mean-luminance jumps of 123 out of 255,
and nine frames in the short jumped more than 25. Cutting them for dissolves took the
worst jump to 24 and the count over 25 to zero. If you want the beat, use `dip` — it goes
through black rather than white and lands around 20.

`promo-edit.py` now refuses to publish a cut that trips a miniature WCAG 2.3.1 — any
single-frame jump over 60/255, or more than three jumps over 25/255 inside one second —
and prints the worst jump on every successful run, so this cannot regress quietly.
`--allow-flashes` is the deliberate override.

**Let the camera settle into the cut.** A key's `ease` describes the blend _into_ it, so
the last camera key of a shot decides whether the move is still at full speed when the
join lands. `linear` there means the frame is still travelling when the next shot starts
underneath it, and two moving frames dissolving through each other is most of what reads
as a "harsh" cut. `smooth` on the final key eases both ends and is the calm default.

Two framing numbers are worth copying. The ground scatter is authored for the RTS camera,
so a lens at `pitch` 8-13 with `height` around 1 m films the field through a wall of grass
and plant billboards; **`pitch` 16-25 with `height` 1.5-2.6** looks over the cover and
still holds the animals large in frame. And when you change the locked hour, **measure the
result rather than looking at it**: moving this reel from 13.0 to 16.9 cost 35% of the
scene's light (mean frame grey fell from ~110 to ~70) and read as murk. It sits at 15.2
with `exposure_override` 1.5, which puts the poster frames back in the 95-107 band.

```sh
python3 -c "import subprocess;raw=subprocess.run(['ffmpeg','-v','error','-i','01_village.png',
  '-vf','scale=192:108,format=gray','-f','rawvideo','-'],capture_output=True).stdout;
  print(sum(raw)/len(raw))"
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

## The master trailer

`tools/arena/promos/trailer.json` is the two-minute trailer, and it differs
from every other reel in the directory in one structural way: **it cuts across
scenarios**. A reel films one scene; the trailer cuts nineteen of them, so a
shot's `scenario` field changes down the list and the runner plans one
deterministic pass per scenario.

Five of those are the valley chapters in
`tools/arena/arena_trailer_scenarios.cpp`, which share `dress_valley` -- one
river valley with a village on the north bank, a timber fort on the eastern
rise, one bridge, and open ground to the south:

- `trailer_dawn` -- the economy and the ambient life: residents on errands,
  woodcutters and quarriers, the flock, birds, and a wolf pack released as a
  timed wildlife wave.
- `trailer_muster` -- the fort turns out, the column crosses the bridge, and
  the army deploys from column into line.
- `trailer_clash` -- the pitched battle, elephants and cavalry included.
- `trailer_pov` -- the same fight from behind the commander's shoulders under
  `rpg_mode`.
- `trailer_barrow_night` -- the night ambush, driven by a real `undead_zone`.

The valley is the trailer's home ground, not the whole of it. A cut that never
leaves one stage advertises one stage, so the valley is interleaved with nine
chapters that are somewhere else and look it -- `trailer_works` on broken stony
soil, `trailer_sanctuary` and `trailer_gate_march` and `trailer_siege_walls` on
dry Mediterranean grass, `trailer_bridge_defense` on farmland, `trailer_highland`
and `trailer_night_snow` and `trailer_last_breath` on alpine rock under snow,
`trailer_forest_ambush` in the wet pine wood -- plus shots lifted straight from
the standing catalog (`carthage_trade_town`, `riverside_mill_town`,
`promo_rome_iron_line`, `promo_carthage_crescent`, `promo_storm_charge`,
`promo_commander_duel`). Each chapter names its own `ground_type`,
`terrain_seed_override`, `weather` and `precipitation`, so the ground and the
sky are part of the shot list rather than a constant behind it.

Five things about it are worth copying and were all learned the hard way here.

**Author distance, not choreography.** An army walks at about 0.68 m/s and
files across a 4.5 m bridge deck one formation at a time. The first cut of the
muster marched the legion the length of the valley with `FormArmy` column
moves; ninety seconds later it had not reached the water, and the formation had
drifted rather than marched. Stage each chapter where its shots are, and keep
travel to the metres the camera actually watches.

**Scenes need enough health to survive their own shot list.** Every shot is
authored after the beat it films, so a line that is wiped twenty seconds in
takes half the reel with it. The clash and night chapters set explicit
`health_override` values for exactly this reason -- not for balance, but so the
melee is still running when the camera arrives.

**Light is measured, not eyeballed.** The dawn chapter first sat at hour 7.6
and captured at a mean 42/255 against the 95-107 band the other reels hold. It
is at 9.8 with `exposure_override` 1.5 now, and the muster at 11.4 with 1.25.
Sweep the hour with `--time` and measure the frame rather than looking at it.

**Yaw picks the light and the background, so compute it.** The camera offset is
`(sin yaw, _, cos yaw) * distance` from the aim point, so yaw decides which side
of the subject the lens sits on. Get it wrong and you lose the shot twice over.
The first capture of this trailer aimed most of the battle south-west at hour
18.1, which is straight into a low western sun _and_ straight at the edge of the
arena floor: the subjects came back as silhouettes and the boundary terrain
filled the top third of frame with a grey wall. `hosts_face` measured 32/255.
Re-aimed west-looking-east -- lit side toward the lens, the valley behind the
action -- the same shot measures 56 and has the river, the bridge and the fort
in the background. Evening acts want yaw 240-300; the morning act wants the
opposite, 60-120.

**A commander's facing is a camera decision.** `facing_degrees` 0 points at +z
and 180 at -z, `RpgMove` axes are read in the commander's view frame, and the
enemy therefore has to stand in front of whichever way he faces or he walks away
from the fight he was given. But the chase camera cannot be aimed independently
-- it sits behind him -- so his facing is also the only control over what is in
the background. `trailer_pov` faces 180 and puts its enemies at lower z purely
so the shot looks back down the valley at the bridge and the village instead of
at the boundary.

### Act cards are rendered, not filmed

A shot with `"flame_card": true` replaces the frame with a procedural wall of
fire drawn by `ArenaViewport::render_flame_card` -- a domain-warped fbm scrolled
upward, plus sparks and a smoke haze, as one fullscreen triangle. It takes
`flame_speed` and `flame_intensity`, needs neither `focus` nor `camera`, and is
a pure function of a frame counter, so re-recording a card reproduces it
exactly.

This exists because the engine's own fire is authored at prop scale. A camp
fire or a burning roof reads as a small bright object; an act card wants a
field the whole frame can sit in. Several earlier attempts to build the card
out of shipped fire are recorded here so nobody repeats them: a structure's
simulation fire only lights when incendiary damage passes five percent of its
max health -- so giving a house 30000 hit points puts ignition out of reach --
a catapult more than about twenty metres from its target never fires at all,
and `suppress_terrain_features` puts out every world prop, fire camps included,
because props ride the same `include_features` flag as the rest of the terrain
decoration.

`promo-edit.py` sets a shot's `act_title` (with an optional smaller
`act_kicker` above it) large and centred for the whole shot, which is the
Praetorians-style interstitial the cards are modelled on. The spec-level
`end_card_seconds` lengthens the closing title hold beyond the two seconds a
short reel uses.

### The score

Every shipped track is exactly 90 s, so a 120 s trailer cannot be scored from
one of them. `scripts/make-trailer-score.sh` assembles six windows into one
piece whose section boundaries land on the act cards and writes it to
`artifacts/promo/trailer_score.ogg`. It is derived, so it is not checked in;
`promo-edit.py` still runs it through the game's own audio mastering.

### Sound

`scripts/place-trailer-cues.py` writes the `sfx` list. It does not read a trace
-- there is no single trace for a reel cut from five scenarios -- so its cues
are authored against **shot names and offsets within a shot** and resolved onto
the blended timeline. Retiming or reordering a shot therefore moves its cues
with it, which is the failure mode hand-typed absolute times always hit. Re-run
it after any change to the picture and before the edit.

### Running it

The spec records at `supersample: 2`, so the arena renders 3840x2160 and
downsamples. The art is low-poly and full of hard silhouette edges that alias
badly at 1080p; the extra pixels are the cheapest quality the reel gets.

```sh
scripts/make-trailer-score.sh
build/bin/arena_app --promo-spec tools/arena/promos/trailer.json \
  --promo-out artifacts/promo
scripts/place-trailer-cues.py
scripts/promo-edit.py --spec tools/arena/promos/trailer.json \
  --clips artifacts/promo/trailer
```

### Fixing one act without re-recording the reel

At `supersample: 2` a full capture is most of an hour, and a shot that needs a
tighter lens or a retimed beat should not cost the other twenty-seven.
`scripts/reshoot-promo-scenario.py <scenario>` records only the shots that name
one scenario and copies each result back over the clip with the matching
_name_ -- the clip index, the posters and `shots.json` are left alone, so
`promo-edit.py` picks the new footage up with nothing else changed.

```sh
scripts/reshoot-promo-scenario.py trailer_muster
scripts/promo-edit.py --spec tools/arena/promos/trailer.json \
  --clips artifacts/promo/trailer
```

Re-run `place-trailer-cues.py` as well if the re-shoot changed a shot's
`duration` or `slow_motion`, because that moves everything after it.

`reshoot-promo-scenario.py` will also _fill in_ a shot the reel does not have
yet, naming it by its index in the full spec, so a reel can be built one
scenario at a time. The arena only writes `shots.json` when a whole run
finishes, though, so a reel assembled that way has footage and no manifest;
`scripts/make-promo-manifest.py` writes one by measuring the clips that are
actually on disk. Measuring rather than trusting the spec is deliberate -- a
clip that came out short should show up as short rather than be assumed
correct.

At `supersample: 2` the battle chapter is the expensive one: each pass
re-simulates the whole window at 3840x2160 with four hundred-odd soldiers, and
overlapping shot windows force _additional_ passes over the same scenario.
Watch for that when authoring -- moving one shot out of an overlap took this
reel's battle from three passes to two.

**Do not rebuild while a capture or a batch loop is running.** Relinking
`arena_app` mid-run makes the next invocation fail to launch, and because it
never reaches a PASS/FAIL line the run just looks like it stopped early.

### Judge the light by measuring it

Every shot in the first cut of this trailer was framed by eye and half of them
came out unusable. Two numbers catch it before a capture is wasted:

```sh
# mean luminance of a finished clip, 0-255
ffmpeg -v error -i 15_hosts_face.mp4 -vf fps=4,scale=96:54,format=gray -f rawvideo - \
  | python3 -c "import sys;d=sys.stdin.buffer.read();print(sum(d)/len(d))"
```

Daylight acts want 70-100. The battle sits around 55-70 by design and the night
act lower still, but 10 is not a mood, it is a black frame -- that is where the
night chapter started before its exposure and its fires were added. Measure the
whole clip rather than the poster: the poster is the shot's _last_ frame, after
the camera has finished moving, and routinely reads ten to twenty points darker
than the shot it represents.

## Render defects this reel exposed

A trailer holds a shot for four seconds at 1080p, which is a far harsher test
than play. Four engine defects only became visible that way, and all four are
fixed in the renderer rather than worked around in the shot list.

**Soldiers popped in and out at the frame edge.** The per-soldier frustum test
had no hysteresis, so a body sitting within centimetres of a plane flipped as
the camera moved -- measured on the commander's chase camera, sixty-six
soldiers flipped eighty-seven times in sixteen seconds, some twice inside two
frames. `render/humanoid/runtime/instance_prepare.cpp` now remembers which soldiers it drew recently
and requires a wider margin to drop one than to pick it up, which took the same
run to twenty flips with nothing oscillating. It costs nothing measurable: the
`performance_30v30` contract still runs at a p95 of 1.4 ms.

**Buildings appeared to blink.** They were not blinking; birds were flying
between the lens and them. A bird is only half a metre across, but the flocks
cruised at camera height, and one passing a metre from the lens fills a third
of the frame for two frames. The bird renderer now drops any bird closer than
three metres to the camera, and the trailer's flocks cruise at fourteen to
fifteen metres. Eighty-four one-frame reversals in an eight-second dolly became
zero. `select_render_archetype_lod_stable` additionally gives building LOD the
same hysteresis, so a structure sitting near its detail threshold cannot thrash
between its full and minimal mesh.

**Sheep came apart when attacked.** Each joint of the head chain was displaced
on its own -- the withers by the body's bob, the poll and muzzle by a separate
head nod, and during the death collapse by two different descents with
different bounce weights -- so the neck bone stretched by the difference and the
head tore off the body, worst at a flee gait, which is exactly when a wolf is
on them. The head group is now reattached rigidly to the poll and the poll held
one neck length from the withers.
`tests/render/sheep_rig_integrity_test.cpp` asserts the bone lengths hold
across the gait, the graze and the whole death collapse.

**Troops walked through walls.** The arena never registered spawned buildings
with `BuildingCollisionRegistry`, so no arena scenario has ever had buildings in
its navigation grid. That is fixed, but it was only half the story: the valley's
bridge approach ran straight through a house, so the column had no legal ground
either way. See the settlement layout notes in `tools/arena/README.md`.
