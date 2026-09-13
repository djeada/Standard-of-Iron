# Promo Capture

Promotional video production is split between Arena capture and the offline editor:

- `arena_app --promo-spec` renders authored scenarios/camera moves and writes shot clips;
- `scripts/promo-edit.py` assembles those clips into a graded, captioned, scored final cut.

Both stages read the same promo spec, so shot timing/camera data and editorial presentation are authored together.

```sh
build/bin/arena_app \
  --promo-spec tools/arena/promos/rome_iron_line.json \
  --promo-out artifacts/promo

scripts/promo-edit.py \
  --spec tools/arena/promos/rome_iron_line.json \
  --clips artifacts/promo/rome_iron_line
```

The Arena capture path requires a working graphics context. The edit path requires `ffmpeg`.

## Promo spec

The Arena-facing schema is defined by `tools/arena/promo_spec.h`.

A `Spec` contains:

- ID/title;
- output width/height/FPS/supersampling;
- audio/music/report-card settings;
- gameplay UI/casting-overlay switches;
- decision/world-edge requirements;
- motion limits; and
- an ordered shot list.

Each `Shot` contains:

- scenario and seed;
- time-based `start_seconds` or event-based `start_on`;
- duration;
- slow-motion factor;
- shake;
- gameplay/casting/RPG-HUD/UI flags;
- report-card duration;
- focus rule; and
- camera keyframes.

## Focus modes

`FocusMode` currently supports:

- `Point`;
- `Group`;
- `GroupPair`;
- `AllUnits`;
- `Battle`; and
- `Army`.

A focus can also carry an offset, owner, engagement/home radii, and smoothing value.

## Camera keys

A camera key contains:

- time;
- distance;
- pitch;
- yaw;
- field of view;
- roll;
- height; and
- easing mode.

Easing modes are `Linear`, `Smooth`, `EaseIn`, and `EaseOut`.

`Promo::evaluate()` resolves the camera pose for a shot time. `motion_violations()` validates the authored camera path against `MotionLimits` such as yaw/pitch/FOV/roll speeds, roll magnitude, shake, and clip-length constraints.

## Event-driven starts

A shot can start relative to one of the timeline events recognized by `promo_spec.h`:

- `first_wave`;
- `first_contact`;
- `first_building_lost`; or
- `decision`.

`StartOn` can also select a side and apply a positive or negative offset.

Specs that use event-driven starts require a timeline/precheck path so the events can be resolved to concrete shot times before capture.

## Decision and world-edge requirements

`Spec::require_decision` requires the referenced scenario to reach a battle decision before the recorder accepts the capture plan.

`Spec::forbid_world_edge` turns camera/world-edge framing violations into a failed capture rather than a warning. `view_ground_footprint()` and `frames_world_edge()` implement the geometry check used by the promo tooling.

## Matchup shorts

Arena can build a self-contained matchup capture from a textual force description:

```sh
build/bin/arena_app \
  --matchup "20 swordsman vs 20 archer" \
  --promo-out artifacts/promo
```

Nation-qualified examples are also accepted by the matchup parser. The generated matchup creates its scenario/spec in memory, records the fight, and uses the matchup report-card style for the result.

`--matchup-seconds` is the fight ceiling; a matchup whose battle-decision expectation resolves earlier can finish before that ceiling. `--matchup-report-seconds` controls the closing report-card hold.

## Gameplay UI capture

Promo capture can include or omit gameplay presentation independently per spec/shot.

Renderer-owned world markers are controlled through the capture/cinematic rendering path. QPainter-based overlays such as floating gameplay numbers are composited onto the captured image by the Arena viewport capture path rather than relying on the on-screen widget framebuffer.

`gameplay_ui_all_owners` allows spectator-style captures to show qualifying owner markers for more than the local side.

## Casting overlay

`casting_overlay` enables the broadcast-style match strip used by cast AI-versus-AI captures. It reads live battle-side census/economy/state data from the running scenario rather than from pre-authored overlay numbers.

## Scenario passes

`Promo::plan_passes()` groups shots into capture passes by scenario/seed. Shots can therefore reuse a scenario definition while retaining independent camera/timing configuration.

Shot capture re-simulates the scenario from its deterministic setup for the pass rather than treating previous shot footage as authoritative world state.

## Slow motion and time-lapse

`slow_motion` changes the simulation-to-screen time relationship for a shot while keeping simulation steps bounded by the recorder's stepping path.

The JSON loader also supports the time-lapse form used by the current promo specs; a shot cannot simultaneously request incompatible time-scaling forms.

## Transitions and editing

`scripts/promo-edit.py` owns editorial fields that Arena does not need to interpret, including titles/captions, grade, music, and transitions.

The editor's `TRANSITIONS` table is the source of truth for transition vocabulary. A shot transition describes the join into that shot; a `cut` selects a hard join. Blended transitions overlap adjacent clips, so the final timeline accounts for transition overlap when placing captions and audio.

Command-line transition overrides can replace the spec-level transition choice for comparison/re-cutting without re-rendering the Arena footage.

## Audio

When promo audio is enabled, Arena records the game's offline mix for the scenario. The editor can combine that source audio with the promo's authored music/report sounds and applies the spec's loudness/music settings during the final cut.

## Formation promo runner

`scripts/capture-formation-promos.sh` runs the capture/edit workflow for the shipped formation reels. Its edit-only path can rebuild the final cut from existing shot clips without re-running Arena.

## Capture determinism

Only sampled capture frames are allowed into encoded clips. Window-system repaint requests are not treated as authored capture samples.

The promo path also performs render warm-up before recording a shot from a freshly loaded scenario so capture does not begin before the scenario's render resources have passed through the required preparation frames.

## Reviewing artifacts

The capture output includes shot metadata used by the tooling to validate framing/timing and to assemble the final edit. Cast/precheck paths also write timeline information for resolved match events.

The promo spec and copied/generated artifacts are the reproducible description of the cut. Editorial claims should be derived from those artifacts and the current tooling rather than from notes about an earlier reel revision.

## Source of truth

The current promo pipeline is defined by:

- `tools/arena/promo_spec.h` and its implementation — spec schema, camera evaluation, pass planning, world-edge and motion validation;
- Arena promo/capture code — scenario simulation, capture, audio/UI composition, report cards;
- `scripts/promo-edit.py` — joins, grade, captions, music, transitions, final encoding; and
- `scripts/capture-formation-promos.sh` — formation-reel orchestration.
