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

## Matchup shorts

One flag records a whole vertical short from a sentence:

```sh
build/bin/arena_app --matchup "20 swordsman vs 20 archer" --promo-out artifacts/promo
```

A side may name its nation, either before or after the unit, and "units" is
allowed as noise:

```sh
build/bin/arena_app --matchup "2 carthage swordsman vs 5 iron sepulcher swordsman"
build/bin/arena_app --matchup "swordman carthage 2 units vs swordsman rome 5 units"
```

It builds a scenario and a promo spec in memory rather than reading either off
disk: two lines on opposite sides of the lens, a locked camera close enough to
read a soldier, the gameplay UI on, and a closing battle report that names the
winner. The output is 1080x1920 at 30 fps, which is what YouTube Shorts wants.

Five things about it are worth knowing:

- **A unit is a unit.** Groups carry no `individuals_per_unit` override, so each
  one fields the headcount and the formation layout its troop profile defines --
  the same one the game puts on a map -- and carries one mode indicator, because
  the indicator belongs to the unit. "2 units" is two formations, not two men.
- **Nation, not just troop.** A Carthaginian swordsman and an Iron Sepulcher
  swordsman are different soldiers; the nation reaches the group, the spec id and
  the report card. Two sides that both leave it unsaid get opposing powers, so
  they cannot read as one army in two colours.
- **The armies close across the frame.** The lens sits on the Z axis (`yaw 0`),
  which puts the separation between the armies across the narrow side of a 9:16
  clip -- left to right and right to left -- and runs each line into the depth of
  the frame, where the tall side has room for it. Distance is computed from those
  two extents and the shot's field of view, so a bigger side is framed from
  further back without anyone touching a keyframe.
- **The lens is locked.** Focus is a fixed point, not the melee centre: the
  centre jumps every time a unit on one flank dies, and a camera chasing it reads
  as handheld shake. Only the distance moves, creeping in as the lines meet and
  the separation stops needing to be in frame.
- **It carries the game's own audio.** `spec.audio` is on, so `AudioRecorder`
  runs the real mixer offline against the same world -- steel, shouts, the
  ambience that follows the fight in and out of combat -- writes it as a wav and
  muxes it into the clip. It needs `ffmpeg`, the same as the picture does.
- **The short ends on the kill, not on the clock.** The generated scenario
  carries a `BattleReachesDecision` expectation, which is what lets the runner
  stop two seconds after one side is wiped out and cut to the report card.
  `--matchup-seconds` is the ceiling on the fight, not its length.

- **Both armies carry their markers.** Order markers are normally drawn for the
  local owner alone, which in a spectator matchup left every icon over one army
  and none over the other. `gameplay_ui_all_owners` -- set by the matchup builder,
  and available to any spec -- lifts that to every owner.

`--matchup-report-seconds` holds the closing card longer or shorter, `--seed`
changes the run, and `--promo-out` picks the directory. The clip lands under
`<promo-out>/matchup_<a>_<unit>_vs_<b>_<unit>/`.

The closing card is its own layout (`ReportCardStyle::Matchup`), not the reel's:
the reel card puts its two sides in columns, which a 1080-wide frame cuts in
half, and it reports an economy a matchup does not have. The matchup card stacks
the two sides down the frame with a survivor bar each, and reads
"ARCHERS WIN / 13 OF 20 LEFT STANDING".

## The gameplay UI in a clip

A recorded frame carries the presentation a player sees over the battle: the
floating damage, healing and economy numbers, and the mode indicators over units
that are building, gathering, carrying or stalled. `gameplay_ui` turns that off
for a reel that wants the world alone -- `false` on the spec is the default for
every shot in it, and a shot may set its own either way.

Two different mechanisms carry it, which is why it used to be missing from a
recording even though it was on screen in the arena window:

- **The mode indicators are drawn by the renderer**, and `set_cinematic_mode`
  hides every order marker -- indicators, rally flags, patrol flags. Promo mode
  used to set it unconditionally, so no recording could ever contain one.
- **The floating numbers are painted with `QPainter`**, and a widget painter
  paints into the widget's own framebuffer, never into the capture FBO the
  encoder reads. Painting them in `paintGL` would therefore have shown them in
  the preview window and in nothing else. `ArenaViewport::paint_capture_gameplay_ui`
  paints them onto the captured `QImage` instead, after the supersample
  downscale, so their size is measured against the frame that ships rather than
  against the window the arena happens to be running in.

A flame card records neither: it is a full-frame effect pass with no world
behind it.

## Casting a whole match

A cast AI-versus-AI match is a different job from a formation reel: the shots
have to follow a fight whose timing nobody knows in advance, the town building
takes minutes that nobody will watch in real time, and a match that ends in a
stalemate is not worth rendering at all. Four spec features carry that, and
`tools/arena/promos/ai_war_of_towns_cast.json` uses all of them.

**The dry run.** `"require_decision": true` makes the recorder play every
scenario the spec names through once before capture, with rendering suppressed
-- a thirty-minute match takes a minute or two -- and refuse to record when the
battle reaches no decision. `--promo-precheck` forces the dry run for any spec,
`--promo-precheck-only` stops after it. Either way the recorder writes
`timeline.json` beside the clips: when the first wave committed (overall and per
side), when the armies first met, when the first building fell (overall and per
side), and when the match was decided, plus each side's closing census.

**Event-driven starts.** A shot may name a match event instead of a second:

```json
{
    "name": "first_blood",
    "duration": 5.0,
    "start_on": { "event": "first_contact", "offset": -1.0 }
}
```

`event` is one of `first_wave`, `first_contact`, `first_building_lost` and
`decision`; `side` narrows the first two per-side events to one label; `offset`
may be negative. The recorder resolves every `start_on` from the dry run's
timeline before planning its passes, so the rest of the pipeline sees ordinary
`start` values. A shot whose event never happened fails the run by name.

**Time-lapse.** `"time_lapse": 25` shows twenty-five simulation seconds in one
screen second; it is `slow_motion` written the friendly way round for values
below one, and a shot may not set both. The arena sub-steps the simulation so no
system ever sees a tick longer than a thirtieth of a second, and the audio bed
keeps to real time under a time-lapse rather than compressing minutes of battle
into a roar. Camera rate limits are judged in screen seconds, so a pan that is
calm on the clock but whips across a time-lapse is refused.

**The casting strip.** `"casting_overlay": true` (spec-wide, or per shot) paints
the broadcast strip along the top of every frame: each side's name under its
colour, army and soldier count, town size, treasury, doctrine and state, a match
clock, and a strength bar split between the sides by living soldiers. It is the
same data the closing report card prints, drawn from the scenario runner's live
census (`ArenaScenarioRunner::live_battle_sides`) and the session economy, in the
card's typography.

**The world edge.** The first full-match render framed the whole arena from
130 m, and the corners of every frame showed the boundary mountains and the fog
box behind them. `view_ground_footprint` in `promo_spec.h` projects a pose's four
frame corners onto the ground; a frame "shows the world edge" when a corner
looks over the horizon or lands beyond the scenario's flattened floor
(`arena_floor_half_extent`, 58 m on the duel maps) plus a small margin.
Point-focus shots are checked from their keys before capture
(`world_edge_violations`); every recorded frame is checked at capture time and
the count lands in `shots.json` as `edge_frames`. `"forbid_world_edge": true`
turns that warning into a failed run.

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

## The opening proof

`tools/arena/promos/trailer_open_proof.json` over `trailer_open` is the eight-second
proof of the next trailer's first promise -- order the army, drop into the
commander's shoulder, fire the command aura, watch the line answer -- and it is the
standard the rest of that trailer is cut to. Four shots over one scenario: a crane
over both lines at the river (`field_order`), a continuous dive that ends on the
chase camera's exact pose (`into_the_saddle`), the chase camera itself
(`commander_view`, `gameplay_camera`), and a pullback from that same pose
(`the_line_answers`). Five things about it are worth copying.

**Match the cut to the chase camera by measuring it, not by eye.** The chase rig
sits behind the commander with a framing that depends on whether enemies are near
(`Explore` versus `Melee`), so its pose is not a constant. `--batch` the scenario
and read `commander.camera` out of `trace.jsonl` at the cut: `target_resolved -
commander_position` is the shot's `focus.offset`, `|eye - target|` is `distance`,
`asin(dy / distance)` is `pitch`, `atan2(dx, dz)` is `yaw` and `fov` is the rig's
own. A dive whose last key carries those numbers, and a pullback whose first key
does, cut against the `gameplay_camera` shot without a visible jump.

**A widescreen trailer names its own motion ceilings.** The phone-short limits
(12 deg/s yaw, 6 deg/s pitch and fov, 1.5 s clips averaging 2 s) refuse a crane-to-
shoulder dive outright. A spec-level `motion_limits` object raises only the
ceilings it names, in both the arena (`Arena::Promo::load`) and `promo-edit.py`:

```json
"motion_limits": { "pitch_degrees_per_second": 16, "fov_degrees_per_second": 20,
                   "yaw_degrees_per_second": 40, "mean_clip_seconds": 1.8 }
```

The shorts keep the defaults; nothing without the key changes.

**The south plain is narrower than the floor.** `dress_valley` levels 56 m, but
the walkable ground south of the river ends around `z` 30 at `x` -14 and every
spawn is snapped to the nearest walkable cell, so a commander authored at `z` 35
stood inside his own line at `z` 30.5. Read the spawned extents out of the trace
(`units[].position` at 0.05 s) before authoring a camera against an origin.

**Protect the frame in the scenario, not the shot list.** The first crane was
dwarfed by the Carthaginian camp in the foreground and the first chase frame had
the Punic city rampart across its right third. `ValleyOptions::south_camps` and
`plain_pines` turn both off for this scenario, `default_wildlife` keeps the bird
flocks out of the lens, and the engagement sits 12 m west of the bridge road so
the chase camera's 100-degree horizontal field holds the river, the bridge and
the walled town and nothing nearer.

**The aura has to be visible before it can be filmed.** The command aura used to
draw one healer dome at intensity 0.10 -- invisible in evening light -- and a
per-soldier dome the shader caps at alpha 0.12. `render_commander_auras` now
draws an expanding pulse ring on activation, a boundary ring at `aura_radius`,
and a gold ground ring under every buffed soldier, all through `ground_marker`.
The buff itself was always applied (`commander_aura_buffed` in the trace); only
the picture was missing.

The reel is cut with `gameplay_ui` on, so the mode indicators and floating
numbers are the game's own. What the arena cannot record is the QML HUD, the
formation planner's slot ghosts and selection rings, so the "real UI" beats of
the trailer that need those come from the game itself, not from this pipeline.

## Filming the real game

The arena cannot record the HUD: it builds no QML engine, so a shot of the
resource bar, the orders panel, the formation planner or a selection ring has
to come from `standard_of_iron` itself. `--film` does that, and
`scripts/film-game.sh` wraps it:

```sh
scripts/film-game.sh out.mp4 \
  --mission-file tools/arena/promos/film/river.mission.json \
  --action-fixture tools/arena/promos/film/river.action.json \
  --fps 60 --seconds 32
```

**One simulation step per frame.** The simulation thread stays stopped and
`GameEngine::film_step` advances the world by exactly `1/fps` per grabbed
frame, so the footage is 60 fps however slowly the machine renders it, and the
scripted actions fire on the same clock rather than on the wall. That is the
whole reason this works: a 1080p frame costs about 70 ms on this box, and the
result is still smooth 60 fps footage.

**The action fixture is the script.** `--action-fixture` takes the same
versioned format the frame-pacing benchmark uses, and both now dispatch through
`App::Core::apply_benchmark_action`, so the vocabulary grew rather than forked:
camera (`camera_look_at`, `camera_zoom`, `camera_orbit`, `camera_follow`),
placement (`formation_begin/drag/end`, `build_start/hover/place`), commander
(`commander_enter/exit/aura/attack/heavy/special/vanguard/dodge/key_down/key_up`),
plus `game_speed`, `pause`, `select_id`. Any action may be suffixed `_world`
and take `x,z` world coordinates in its argument; the dispatcher projects them
through the live camera, which is what lets a fixture aim at a building site or
a unit without guessing screen pixels.

Four things about the window were each measured before being fixed, and all
four are load-bearing:

- **Xvfb is 20 s per 1080p frame** under llvmpipe. `--display xvfb` still works
  for very short clips; everything else films on the GPU display.
- **A hidden window cannot be grabbed.** Qt tears the scene graph down after
  every `grabWindow` of an unexposed window, and the gameplay renderer refuses
  to re-initialise. `QT_QPA_PLATFORM=offscreen` never builds it at all.
- **An occluded window costs five seconds a frame.** The threaded render loop
  spends them inside the NVIDIA driver in `QRhi::endFrame`, swapping a window
  the compositor is throttling. The film window is therefore parked almost
  entirely off the bottom-right of the screen -- unmanaged, unfocusable, never
  covered -- and `QSG_RENDER_LOOP=basic` grabs on the GUI thread.
- **Only grabs may render.** An event filter eats the window's own
  `UpdateRequest` once the match is loaded, so nothing presents between frames.
  It has to stay off during loading: the loading overlay counts five presented
  frames before it lets the match begin.

`QSG_FIXED_ANIMATION_STEP=1` keeps QML animations on the film clock. QML
`Timer`s still run on the wall clock, so a HUD element driven by one drifts
against the picture; nothing in the shots below depends on that.

The throwaway `XDG_CONFIG_HOME` the script writes also silences the hint cards
(`ui/camera_legend_seen=true`, `economy_coach=false`, `formation_hints=false`)
-- the camera legend otherwise sits over the right third of every frame.

### Filming the editor

`scripts/film-editor.py` drives `map_editor` on its own Xvfb display and records
it with `ffmpeg -f x11grab`. The editor is a widget app, so software GL is fine.
Input goes through python-xlib's XTEST extension (there is no `xdotool` here),
and a 0.4 s dwell is inserted between a move and a press because Qt otherwise
processes the press at the previous pointer position. Steps are JSON: `click`,
`dblclick`, `drag`, `wheel`, `key`, `type`, `sleep`, `record`, `stop`,
`screenshot`. The whole folder of JSON beside the opened file is copied into the
session directory first, because a mission names its battlefield map by a
sibling path.

### The clips a spec did not record

`trailer_v2.json` cuts arena shots together with footage filmed from the game
and the editor. A shot with a `clip` path instead of a `scenario` is skipped by
the arena entirely and laid on the timeline by `promo-edit.py`, which measures
its length off the file. `scripts/trailer-v2-clips.py` is the in/out list that
cuts the long recordings down to the clips the spec names, and
`scripts/place-trailer-v2-cues.py` writes the `sfx` list against shot names the
same way `place-trailer-cues.py` does -- including the commander voice lines,
which are real assets under `assets/audio/voices/` and play in game when a
commander is selected.

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
