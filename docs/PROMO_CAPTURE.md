# Promo Capture

Standard of Iron's promotional video pipeline is split into two reproducible stages: deterministic gameplay capture in Arena and offline editorial assembly with FFmpeg.

Arena is responsible for producing the actual gameplay footage from authored scenarios, seeds, camera moves, timing rules, and capture options. `scripts/promo-edit.py` is responsible for turning those shot clips into the final edited deliverable: joins, transitions, grade, captions, music, loudness, and report-card presentation.

Both stages read the same promo specification, which keeps camera/timing intent and editorial intent in one versioned artifact.

```text
promo spec
   │
   ├──────────────► Arena capture
   │                 │
   │                 ├─ scenario/seed
   │                 ├─ event timeline
   │                 ├─ camera evaluation
   │                 ├─ gameplay UI/casting
   │                 ├─ offline game audio
   │                 └─ encoded shot clips
   │
   └──────────────► promo-edit.py
                     │
                     ├─ trim/join
                     ├─ transitions
                     ├─ captions/titles
                     ├─ grade
                     ├─ music/report sounds
                     └─ final encode
```

The capture path requires a working graphics context. The edit path requires `ffmpeg`.

## Basic workflow

Capture the authored shots:

```sh
build/bin/arena_app \
  --promo-spec tools/arena/promos/rome_iron_line.json \
  --promo-out artifacts/promo
```

Then assemble the final cut:

```sh
scripts/promo-edit.py \
  --spec tools/arena/promos/rome_iron_line.json \
  --clips artifacts/promo/rome_iron_line
```

The capture and edit phases are intentionally separable. If gameplay footage is already correct, editorial changes such as captions, transition choice, music balance, or grade can be iterated without re-simulating the battle.

## Why promo capture is data-driven

A promo reel is difficult to reproduce when its framing exists only as manual camera motion or notes.

The promo spec turns the important decisions into data:

- which scenario is shown;
- which deterministic seed is used;
- when each shot starts;
- what the camera follows;
- how the camera moves;
- whether gameplay UI is visible;
- whether cast overlays are visible;
- whether a battle decision must occur;
- whether world-edge framing is acceptable; and
- how the final edit is assembled.

That makes a shot reviewable in source control and repeatable on another machine using the same runtime/content revision.

## Promo spec schema

The Arena-facing schema is defined by `tools/arena/promo_spec.h`.

A `Spec` contains high-level capture/edit settings such as:

- ID and title;
- output width;
- output height;
- FPS;
- supersampling;
- audio settings;
- music settings;
- report-card settings;
- gameplay UI defaults;
- casting-overlay defaults;
- decision requirements;
- world-edge requirements;
- motion limits; and
- an ordered list of shots.

The spec is the top-level description of one reel or promo sequence.

## Shot schema

Each `Shot` defines one camera/timeline segment.

Current shot fields include:

- scenario;
- deterministic seed;
- time-based `start_seconds` or event-based `start_on`;
- duration;
- slow-motion factor;
- shake;
- gameplay-UI flag;
- casting-overlay flag;
- RPG-HUD/UI flags;
- report-card duration;
- focus rule; and
- camera keyframes.

A shot can therefore be reviewed as a self-contained instruction: run this scenario/seed, begin at this event/time, follow this subject, evaluate this camera curve, and record for this duration.

## Output dimensions and supersampling

The spec defines output width, height, FPS, and supersampling.

Supersampling lets Arena render at a higher internal capture resolution and downsample for the final clip where the current capture implementation requests it. The authored output size remains part of the spec so different editors do not accidentally create differently framed versions from the same camera path.

FPS is also authored rather than assumed by the editor, keeping camera timing and encoded shot duration aligned.

## Focus modes

`FocusMode` currently supports:

- `Point`;
- `Group`;
- `GroupPair`;
- `AllUnits`;
- `Battle`; and
- `Army`.

A focus can also carry:

- an offset;
- owner selection;
- engagement radius;
- home radius; and
- smoothing.

The focus system separates “what should be centered/tracked” from the camera's authored distance, yaw, pitch, and field of view.

### Point focus

`Point` follows an authored world point/target rather than deriving the framing from a force group.

### Group and group-pair focus

`Group` follows one scenario group. `GroupPair` is useful for framing an interaction between two named groups rather than one side alone.

### All-units / battle / army focus

These modes derive broader framing from the active battle/force context. They are useful when the shot should adapt to where the action actually is while still following a deterministic scenario.

## Camera keyframes

A camera key contains:

- time;
- distance;
- pitch;
- yaw;
- field of view;
- roll;
- height; and
- easing mode.

Supported easing modes are:

- `Linear`;
- `Smooth`;
- `EaseIn`; and
- `EaseOut`.

`Promo::evaluate()` resolves the camera pose for an arbitrary shot time by evaluating the authored keys.

This makes camera motion inspectable and testable. A shot is not dependent on a human reproducing the same mouse movement during every capture.

## Motion limits

`MotionLimits` constrain how aggressively the authored camera can move.

`motion_violations()` validates limits involving areas such as:

- yaw speed;
- pitch speed;
- FOV change speed;
- roll speed;
- maximum roll;
- camera shake; and
- clip-length constraints.

The purpose is to catch a shot that is technically valid JSON but produces unreadable or excessively violent motion.

A motion violation is a spec/capture problem, not something that the offline editor should hide with a transition.

## Event-driven starts

Shots can start from simulation timeline events instead of absolute time.

The current event names recognized by `promo_spec.h` are:

- `first_wave`;
- `first_contact`;
- `first_building_lost`; and
- `decision`.

`StartOn` can also select a side and apply a positive or negative offset.

For example, a shot can begin shortly before first contact rather than at a hard-coded second that only works while unit speeds and path lengths remain unchanged.

## Timeline precheck

Event-driven starts require the capture tooling to resolve the scenario timeline before final shot capture.

The precheck/cast path determines concrete timestamps for the requested events. Those timestamps become the reproducible bridge between simulation events and camera shot time.

If the required event never occurs, the spec cannot silently invent a start time.

## Battle-decision requirement

`Spec::require_decision` requires the scenario to reach a battle decision before the capture plan is accepted.

This is useful for reels that claim to show a complete matchup/result rather than an arbitrary mid-battle excerpt.

A decision requirement is a runtime assertion about the scenario outcome, not merely an editorial caption.

## World-edge framing

`Spec::forbid_world_edge` promotes world-edge framing from a warning to a failed capture.

The tooling uses `view_ground_footprint()` and `frames_world_edge()` to test whether the camera footprint exposes the boundary of the authored world.

This is especially important for promo shots because a normal gameplay camera may occasionally tolerate seeing beyond the useful battlefield edge, while a marketing shot usually should not reveal the finite terrain boundary.

## Scenario and seed ownership

Every shot names a scenario and seed.

The scenario supplies deterministic world setup and scripted actions. The seed controls deterministic variation used by that scenario/runtime path.

Promo capture does not rely on the state left behind by a previous unrelated gameplay session. The scenario is re-established from its authored setup for the relevant pass.

## Capture passes

`Promo::plan_passes()` groups shots by scenario/seed.

This lets multiple shots that share one deterministic setup be captured as a coherent pass while retaining independent shot timing/camera definitions.

The pass planner is an optimization/orchestration layer. Each shot still has its own authored timing and camera contract.

## Deterministic re-simulation

Shot footage is derived from simulation, not from previously encoded footage.

When the capture plan needs another pass, Arena re-simulates the scenario from the deterministic setup rather than using the visual output of an earlier shot as authoritative game state.

This is important because encoded video cannot be used to derive exact subsequent simulation state.

## Capture sampling

Only authored capture samples are allowed into the encoded promo clips.

Window-system repaint requests are not treated as additional authored video frames. The recorder controls which simulation/presentation sample is written to the clip.

This keeps encoded duration and camera evaluation tied to the recorder's timeline rather than to incidental UI repaint behavior.

## Render warm-up

A freshly loaded scenario can require a small amount of renderer preparation before a shot is visually representative.

The promo path performs render warm-up before recording from a newly loaded scenario so capture does not begin while required render resources are still passing through their initial preparation frames.

This is separate from the general mission loading overlay because promo capture is a tool-driven deterministic recording path.

## Slow motion

`slow_motion` changes the simulation-to-screen time relationship for a shot.

The recorder still advances simulation through bounded stepping rather than asking the video editor to fabricate gameplay motion from repeated frames.

Slow motion therefore remains coupled to the simulation/capture timeline.

## Time-lapse

The JSON loader also supports the time-lapse form used by current promo specs.

A shot cannot request incompatible time-scaling forms at the same time. The loader/runtime treats the chosen time-scaling mode as part of shot semantics rather than stacking contradictory multipliers.

## Gameplay UI capture

Gameplay presentation can be enabled or disabled at spec and shot level.

This allows the same scenario to produce:

- clean cinematic footage;
- ordinary gameplay-facing footage; or
- spectator/casting footage with selected overlays.

Renderer-owned world markers are controlled through the capture/cinematic render path.

QPainter overlays such as floating gameplay numbers are composited into the captured image by the Arena viewport path rather than relying on whatever happened to be visible in the on-screen widget framebuffer.

## Multi-owner gameplay UI

`gameplay_ui_all_owners` allows spectator-style captures to show qualifying gameplay markers for more than the local owner.

This is useful for AI-vs-AI or broadcast-style footage where one side should not be privileged simply because the capture tool has a nominal local-player context.

## Casting overlay

`casting_overlay` enables the broadcast-style match strip used by cast AI-versus-AI capture.

The overlay reads current battle-side census/economy/state from the running scenario. It is not filled with pre-authored fake result numbers.

That keeps the overlay consistent with the actual captured simulation.

## Report cards

Promo specs and matchup captures can reserve a closing report-card duration.

The report card belongs to the promo/capture presentation layer. It summarizes the result produced by the scenario rather than deciding the outcome itself.

The edit step can then integrate the report-card hold with music and transitions.

## Matchup shorts

Arena can create a self-contained matchup capture from a force description:

```sh
build/bin/arena_app \
  --matchup "20 swordsman vs 20 archer" \
  --promo-out artifacts/promo
```

Nation-qualified force descriptions are also accepted by the current matchup parser.

The matchup path builds its scenario/spec in memory, runs the fight, records it, and uses the matchup report-card style.

### Matchup duration

`--matchup-seconds` is the maximum fight duration.

If the matchup decision expectation resolves earlier, capture can finish before that ceiling.

`--matchup-report-seconds` controls the closing report-card hold.

This makes quick balance/promo shorts possible without authoring a full JSON scenario/spec for every simple matchup.

## Audio capture

When promo audio is enabled, Arena records the game's offline mix for the scenario.

The offline editor can then combine that source with authored promo music and report sounds.

The capture and edit stages have different responsibilities:

- Arena records what the game produced;
- `promo-edit.py` performs final editorial mixing according to the spec.

This avoids making live audio-device timing part of the encoded promo result.

## Offline editing

`scripts/promo-edit.py` owns editorial fields that Arena does not need for simulation/camera execution.

Those include areas such as:

- titles;
- captions;
- grade;
- music;
- report sounds;
- transitions; and
- final encode assembly.

Keeping editorial work out of Arena means a caption or grade change does not require replaying the battle.

## The Steam end card

Every marketing cut ends by sending the viewer to the Steam store page.
`"end_card": "steam_demo"` fills the card from `END_CARD_PRESETS` in
`promo-edit.py`: the free-demo call to action, the full store URL, a wishlist
line and the GitHub link. Any of those keys the spec also sets wins, so a spec
keeps its own `title`. `promo-edit.py` refuses a card whose text carries no
Steam link, and `scripts/trailer/deliver.py` refuses a cinematic cut whose
closing card lacks one; `"end_card_steam": false` opts out an internal review
cut that is not marketing. `tests/scripts/test_promo_end_card.py` checks every
spec under `tools/arena/promos/`.

The display face has capitals only and no `_`, so a card line that needs
another glyph (the store URL's underscores) is set in the bundled EB Garamond.

## Cuts that never hold still

`promo-edit.py` refuses a cut in which more than one frame in ten changes by
over 6/255, because a frame the eye cannot settle on reads as chaos on a
phone. Close tracking of a marching column trips that by design;
`--allow-motion` publishes such a cut with a warning instead, the way
`--allow-flashes` does for the photosensitivity check.

## Transition semantics

The editor's `TRANSITIONS` table is the source of truth for transition vocabulary.

A transition on a shot describes the join **into** that shot.

`cut` is a hard join. Blended transitions overlap adjacent clips, which means the final edit timeline must account for the overlap when placing captions and audio.

Command-line transition overrides can replace spec-level transition choices for comparison or re-cutting without re-running the capture phase.

## Caption and title timing

Because blended transitions change the effective overlap between clips, editorial timing is computed from the assembled timeline rather than simply concatenating raw clip durations.

This keeps captions and audio aligned with what the viewer actually sees after transitions are applied.

## Grade and output consistency

The offline edit step applies the authored grade/output treatment consistently to the captured clips.

That separation is useful when several shots come from different scenarios or camera conditions: the final reel can have one editorial treatment without modifying the renderer or mission content solely for the video.

The grade is creative, not corrective. Until September 2026 the renderer had no display encode, and every spec carried `brightness` and `gamma` lift to make up for dark footage. The renderer now encodes its own output (see the display encode in `RENDERING_ARCHITECTURE.md`), so those keys were removed from the specs, the `promo-edit.py` defaults are brightness 0 and gamma 1, and each spec's `saturation` was divided by 1.11 to take out the saturation the renderer now adds. Clips captured before that change are still dark: re-capture them rather than re-editing them with the new grades.

## Formation promo orchestration

`scripts/capture-formation-promos.sh` runs the shipped formation-reel workflow.

It supports the normal capture/edit sequence and an edit-only path that rebuilds the final video from existing shot clips.

The formation promos therefore use the same production capture architecture as other promo specs rather than a one-off manual recorder.

## Artifact review

Promo output includes metadata used to verify timing/framing and assemble the final edit.

Depending on the path, artifacts include:

- encoded shot clips;
- capture metadata;
- resolved event timeline information;
- scenario/spec identity;
- report/cast information; and
- final edited output.

The versioned promo spec plus those generated artifacts form the reproducible description of the cut.

## Reviewing a shot

A useful review separates simulation, capture, and editing concerns.

### Scenario problem

Examples: the battle never reaches the intended contact, a unit group is absent, decision never occurs.

Fix the scenario/content/runtime.

### Camera/focus problem

Examples: wrong group centered, excessive yaw speed, world edge visible, action framed too tightly.

Fix the promo spec/camera keys/focus/motion limits.

### Capture-presentation problem

Examples: wrong gameplay UI mode, missing casting strip, renderer not warmed up, incorrect overlay ownership.

Fix Arena capture configuration/path.

### Editorial problem

Examples: caption timing, grade, transition choice, music level, final join.

Fix `promo-edit.py` fields/spec and rebuild from existing clips.

Keeping those layers distinct avoids re-rendering a deterministic battle just to move a caption.

## Excerpts from one capture

A spec whose shots are a subset of a capture's manifest is an excerpt:
`promo-edit.py` picks the named shots, in the spec's order, from the clips
directory it is given. The Feature Spotlight reels use this to cut several 9:16
excerpts from one vertical capture without re-simulating anything; see
"Feature Spotlight series" in `tools/arena/README.md`.

## Filming the game window

`scripts/film-game.sh` films the real game, HUD included. The game only
leaves its loading screen after it has presented frames, and takes on a shared
desktop were seen to stop right after audio preload with no error and no
`SOI_FILM: match loaded` line: once with the monitor asleep, and repeatedly
with another game or Arena window open. The script turns vsync off in the
take's throwaway profile and `-- --film-visible` helped once, but the stall is
not understood; Arena capture renders offscreen and is the dependable path for
unattended footage. `--campaign-mission` takes a `campaign_id/mission_id`
pair, not a path.

`--film-audio` gives a game-window film its sound the way Arena audio capture
does: it sets `SOI_AUDIO_OFFLINE` before the audio system starts, pulls exactly
one film step of 48 kHz stereo from `AudioSystem::render_offline` after every
step, keeps the steps that wrote a frame, and writes `audio.wav` beside the PNGs,
the same length as the footage. A film renders faster or slower than real
time, so recording the desktop's output would drift. The take's profile must not
have `master_volume=0` (a muted playtest profile films silence). Mux with
`ffmpeg -framerate FPS -i frame_%06d.png -i audio.wav -c:v libx264 -c:a aac`.

## Cinematic camera rig

The orbit keys above describe a camera circling a focus. Trailer work also
needs cameras that are _placed_: a lens at shin height inside a shield wall, a
dolly along a marching column, a crane that rises out of the rear ranks. A shot
opts into that with `"rig": "free"`:

```json
{
    "name": "shield_rank",
    "scenario": "cine_field",
    "start": 1.0,
    "duration": 5.0,
    "rig": "free",
    "eye_space": "world",
    "look_space": "world",
    "focus": { "mode": "point", "point": [0, 0, 0] },
    "ground_clearance": 0.3,
    "camera": [
        {
            "time": 0,
            "eye": [-37, 0.62, 62],
            "look": [-45, 0.62, 6],
            "fov": 13
        },
        { "time": 5, "eye": [-37, 0.62, 56], "look": [-45, 0.62, 2], "fov": 13 }
    ]
}
```

- `eye` and `look` are positions. With `eye_space`/`look_space` `"focus"` (the
  default) they are offsets from the resolved focus, so a camera can ride
  alongside a moving group; `"world"` pins them to the ground, so a locked-off
  camera can pan to follow a subject. World heights are above the terrain unless
  `"terrain_relative": false`.
- Free keys, and orbit keys with `"interp": "spline"`, are interpolated with
  time-parametrised cubic Hermite splines, so a camera moves _through_ a middle
  key instead of stopping on it. `"ends": "moving"` (the default) keeps the
  camera travelling at the cut, which is what makes a move feel photographed;
  `"ends": "ease"` starts and finishes at rest.
- `ground_clearance` (metres, default 2.2) is the terrain clearance the cinematic
  lift enforces near the lens. Units render at roughly half scale (a soldier is
  about a metre tall), so low lenses need `0.3`; `near` sets the near plane.
- `handheld` (`degrees`, `frequency`, `seed`) adds smooth operator sway, and
  `jolts` (`at`, `degrees`, `decay`) adds a decaying impact shake. Neither is
  per-frame noise.
- Focus following can be steadied with `"spring": true` (critically damped
  instead of first-order), `dead_zone` (metres the subject may drift before the
  camera reacts) and `lead` (seconds of anticipation along the subject's
  velocity).

## Per-shot lighting

A shot's `lighting` block overrides the scenario's lighting for the length of
the shot, without a rebuild: `hour`, `sun_azimuth` and `sun_elevation` (degrees;
azimuth 90 puts the sun towards +x), `sun_scale`, `sun_color`, `ambient_scale`,
`sky_color`, `fog_color`, `fog_density`, `exposure`, `shadow_strength` and
`shadow_softness`. Keep the sun above about 18 degrees on battle shots: lower
suns stretch every soldier's shadow into long stripes across the field. Match
`fog_color` to the sky at dawn and dusk or the haze reads as grey cloud.

Promo capture refuses to run below Ultra or on a software renderer
(`SOI_PROMO_ALLOW_LOW_QUALITY` overrides it for tests only), and logs the GPU it
rendered on.

## The cinematic trailer

`tools/arena/promos/cinematic/` holds the trailer: one `capture_*.json` per film
set, and `cut.json`, the edit decision list with its looks and complete sound
design. `scripts/trailer/` finishes it; see `docs/TRAILER.md`.

## Vertical takes from the same pass

A shot can record more than one framing of the same frames. `vertical` asks for a
9:16 take (1080x1920 unless it says otherwise) rendered in the same scenario
pass as the 16:9 shot: after every captured frame the arena moves the lens to
the variant's pose, renders the same simulation state a second time into its own
offscreen target, and streams it to a second encoder. Nothing is re-simulated,
so the two takes show the same soldiers in the same places on every frame, and
the vertical take never depends on two runs agreeing.

```json
{
  "name": "phase_one",
  "scenario": "cine_field",
  "start": 16.0,
  "duration": 8.0,
  "focus": { "mode": "point", "point": [-4, 0, 0] },
  "camera": [
    { "time": 0.0, "distance": 125, "pitch": 34, "yaw": 150, "fov": 38 },
    { "time": 8.0, "distance": 110, "pitch": 30, "yaw": 178, "fov": 38 }
  ],
  "vertical": {
    "focus": { "offset": [-18, 0, 0] },
    "camera": [
      { "time": 0.0, "distance": 105, "pitch": 40, "yaw": 120, "fov": 46 },
      { "time": 8.0, "distance": 95, "pitch": 36, "yaw": 140, "fov": 46 }
    ]
  }
}
```

- A variant is the shot's own JSON with the variant's keys laid over it, so it
  can replace `camera`, `rig`, `interp`, `ends`, `eye_space`/`look_space`,
  `handheld`, `jolts`, `shake`, `near`, `ground_clearance`, `casting_overlay`
  and the focus `offset`. It shares the shot's subject (focus mode and group).
- Quick lens changes need no new keys: `fov_scale`, `distance_scale`,
  `yaw_offset`, `pitch_offset` and `offset` (added to the focus offset) adjust
  every inherited key. Keep in mind that `fov` is vertical: a 9:16 frame with the
  16:9 lens is already much tighter horizontally.
- Anything that would need a different simulation is refused: `scenario`,
  `seed`, `start`, `start_on`, `duration`, `slow_motion`, `time_lapse`,
  `report_card`, `flame_card`, `gameplay_camera`, `lighting`, `rpg_hud`,
  `gameplay_ui` and `stabilize_seconds`.
- `variants: [{ "name": "square", "width": 1080, "height": 1080, ... }]` records
  any number of other framings; `vertical` is shorthand for one named
  `vertical`. A spec-level `vertical` block gives every shot a vertical take;
  a shot opts out with `"vertical": false`.
- Variants are checked against the spec's motion limits like any other camera.
- Each variant writes `NN_<shot>.<variant>.mp4`, its poster, its own camera
  track, and the shot's game audio. A report card is repainted at the variant's
  size. `shots.json` lists them under each shot's `variants`.
- `scripts/promo-edit.py --variant vertical` cuts the vertical takes with the
  same spec, so a reel and its episode come from one capture.

Each variant costs one more world render per frame; at Ultra a vertical take
roughly doubles capture time.

## Camera export

Every clip is written with a camera track beside it, `NN_<shot>.camera.jsonl`
(and `NN_<shot>.<variant>.camera.jsonl` for variants). The first line is a
header; every following line is one frame of the clip, in order, with the same
count as the video.

```json
{"type":"soi_camera_track","version":1,"spec":"cine_field_tactical","shot":"phase_one",
 "variant":"","scenario":"cine_field","seed":1337,"clip":"01_phase_one.mp4","fps":30,
 "width":1920,"height":1080,"supersample":1,"slow_motion":1,"start_seconds":16,
 "terrain":"terrain_cine_field_1337.json","groups":["rome_swords","punic_swords"],
 "matrix_layout":"column_major","clip_space":"opengl","pixel_origin":"top_left","world_up":"+y"}
{"frame":0,"t":0,"scene_t":16.033,"shot_t":0.0,"view":[16 floats],"projection":[16 floats],
 "eye":[x,y,z],"target":[x,y,z],"up":[x,y,z],"fov_y":38,"aspect":1.7778,"near":0.5,"far":900,
 "render_size":[1920,1080],"viewport":[0,0,1920,1080],
 "groups":{"rome_swords":{"owner":1,"alive":26,"centroid":[x,y,z],"forward":[x,y,z],
   "front":[[x,y,z],[x,y,z]],"width":104.0,"depth":3.2,"units":[[id,x,y,z,yaw],...]}}}
```

- `view` and `projection` are read from the game camera after the frame was
  rendered, so they are exactly the matrices the renderer used, including the
  cinematic lift over terrain, handheld wobble and gameplay stabilisation. They
  are column-major (Qt and OpenGL order). A world point maps to pixels with
  `clip = projection * view * [x, y, z, 1]`, `px = (clip.x / clip.w * 0.5 + 0.5) * width`,
  `py = (1 - (clip.y / clip.w * 0.5 + 0.5)) * height`. Normalised device
  coordinates do not depend on resolution, so supersampled captures project the
  same way at the delivered size; `render_size` records the internal one.
- `t` is clip time (`frame / fps`), `scene_t` is the scenario clock of the
  rendered state, and `shot_t` is the time the camera keys were evaluated at.
- `groups` carries every living unit of each exported group, the centroid, the
  mean facing (`forward`; yaw 0 faces +z, 90 faces +x), and the two ends of the
  leading rank (`front`, ordered left to right as the group faces). Which groups
  are exported is `overlay_groups` (spec or shot): `"all"` (the default), a
  list of names, or `"none"`.
- Report-card frames carry `"camera": false`.
- The battlefield height field is written once per scenario and seed as
  `terrain_<scenario>_<seed>.json` and a raw little-endian float32 grid beside
  it (row-major, z rows then x columns, 1 m spacing), sampled with the same
  `TerrainService::get_terrain_height` units stand on.

## Tactical overlays

`scripts/tactical_overlay.py` draws documentary map graphics on a captured clip
-- movement arrows, army blocks, frontage lines and labels -- registered to the
world. Elements are authored in arena metres, draped over the exported terrain,
and projected through each frame's own matrices, so an arrow drawn down a
hillside stays on that hillside while the camera moves. Ground elements are
drawn without depth testing: they are never hidden behind soldiers or hills.

```sh
scripts/tactical_overlay.py \
  --clip artifacts/promo/cine_field_tactical/01_phase_one.mp4 \
  --overlay tools/arena/promos/tactical/cine_field_tactical.overlay.json \
  --out artifacts/promo/cine_field_tactical/01_phase_one.tactical.mp4
```

`--alpha-out plate.mov` writes the overlay alone as ProRes 4444 with alpha, and
`--frames DIR --stills 0,120,239` writes single RGBA frames for review. The
conform stage (`scripts/trailer/conform.py`) takes an event's `"overlay"` (a path
or an inline object) and `"overlay_style"`, renders the plate, puts it through the
event's own trim, speed, motion blur, reframe and shake, and lays it over the
picture after the grade so the look never tints the graphics.

An overlay description is a list of `elements`. Every element takes `start` and
`end` (clip seconds), `fade_in`/`fade_out`, `class` (named styles) and `style`
(inline overrides).

| Element    | Fields                                                                                                                                                                                                                    |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `arrow`    | `points` (two or more anchors), `bend` (two points: the apex sits that many chord lengths to the left; negative bends right), `smooth` (spline through 3+ points), `draw` (draw-on seconds), `track` (`"fixed"` at start, `"live"`, or a clip time) |
| `block`    | `group`: a rectangle around the group's living units, oriented by their facing and padded by `padding_m`; its leading edge is drawn heavier. Follows the group live.                                                         |
| `frontage` | `group`: a line along the leading rank, extended by `extend_m`, drawn on from the centre. Follows the group live.                                                                                                          |
| `label`    | `text` and `at` (one anchor), `height_m` above the ground; text is set in the display face and offset in pixels.                                                                                                          |

An anchor is `[x, z]` (or `[x, y, z]`, y ignored) in arena metres, or a group
reference: `{"group": "rome_swords", "at": "centroid" | "front" | "front_left" |
"front_right" | "rear"}`, optionally `"unit": k` (the k-th living unit as first
exported, followed by entity id, so it stays the same maniple), `"forward_m"` /
`"right_m"` along the group's facing, and `"offset": [dx, dz]`.

Styling is injectable. `DEFAULT_STYLE` in the script gives every element a
default (palette, owner colours, metric widths, pixel strokes authored for a
1080-pixel short side, the bundled display face). A style JSON passed as
`--style`, or `style=` from Python, is merged over it -- this is where the
series graphics package plugs in -- then the description's own `style` block,
then any `class` entries from `style.classes`, then the element's inline
`style`. A colour is a palette name, `#rrggbb` or `[r, g, b]`; with no colour,
blocks and frontages take their owner's colour and arrows the owner of their
first group anchor.

Shapes are filled into supersampled coverage masks (Pillow only) and
box-filtered, which is what makes their edges anti-aliased; labels use the
font's own anti-aliasing.

`tools/arena/promos/tactical/cine_field_tactical.json` and its
`.overlay.json` are the worked example: an oblique camera orbiting `cine_field`
while a Roman maniple's arrow runs down the Roman hill to the Carthaginian
centre.

## Long-form films

Half-hour ambience films are captured and finished with two tools:

- `scripts/promo-guarded-capture.py SPEC OUTDIR` captures a long spec in
  chunks, one `arena_app` process per chunk, under a thermal guard that pauses
  the arena on a hot CPU or GPU (an unguarded 30-minute Ultra render once
  powered a workstation off), retries stalled or short clips, and writes the
  `shots.json` that `promo-edit.py` cuts from.
- `scripts/promo-long-film.py` cuts the picture with `promo-edit.py`, builds a
  chapter-by-chapter ambience and music soundtrack, burns in the film and
  chapter titles, normalises to -14 LUFS and writes a YouTube description with
  chapter timestamps. Its docstring documents the spec's `chapters` list and
  the soundtrack file.

A shot that needs the city, camp or village running for half an hour should
not stretch a scenario's scripted steps across the whole film: the scenario
runner slows sharply once a definition carries thousands of steps. Cycle shot
starts through the scenario's own length instead and let the capture planner
split the passes; each shot's `lighting.hour` still carries the film from dawn
to night.

## Reproducibility rules

The current pipeline depends on several invariants:

- scenario and seed identify deterministic gameplay setup;
- shot starts resolve from explicit time or recognized timeline event;
- camera poses come from authored/evaluated keys;
- capture sampling is controlled by the recorder;
- event/precheck requirements must resolve rather than silently default;
- world-edge/motion requirements are validated when requested;
- final editing is derived from the same spec; and
- generated artifacts are evidence of the actual cut.

These rules make the promo pipeline suitable for source-controlled production rather than ad-hoc screen recording.

## Source map

| Concern                           | Source                                                           |
| --------------------------------- | ---------------------------------------------------------------- |
| Promo schema/camera/pass planning | `tools/arena/promo_spec.h` and implementation                    |
| Arena capture                     | `tools/arena/` promo/capture code                                |
| Matchup parser/generator          | Arena matchup path                                               |
| Offline edit                      | `scripts/promo-edit.py`                                          |
| Long-form capture and finishing   | `scripts/promo-guarded-capture.py`, `scripts/promo-long-film.py` |
| Formation reel orchestration      | `scripts/capture-formation-promos.sh`                            |
| Camera export                     | `tools/arena/promo_camera_export.cpp`                            |
| Tactical overlays                 | `scripts/tactical_overlay.py`                                    |
| Authored promo specs              | `tools/arena/promos/`                                            |

The current promo spec, Arena capture implementation, and offline editor are the source of truth for how a reel is produced. Notes about an earlier revision of a trailer are not part of the production contract.
