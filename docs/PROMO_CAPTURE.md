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

| Concern                           | Source                                        |
| --------------------------------- | --------------------------------------------- |
| Promo schema/camera/pass planning | `tools/arena/promo_spec.h` and implementation |
| Arena capture                     | `tools/arena/` promo/capture code             |
| Matchup parser/generator          | Arena matchup path                            |
| Offline edit                      | `scripts/promo-edit.py`                       |
| Formation reel orchestration      | `scripts/capture-formation-promos.sh`         |
| Authored promo specs              | `tools/arena/promos/`                         |

The current promo spec, Arena capture implementation, and offline editor are the source of truth for how a reel is produced. Notes about an earlier revision of a trailer are not part of the production contract.
