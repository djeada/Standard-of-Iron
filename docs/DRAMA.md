# Drama episodes

_Standard of Iron: The Dead Company_ is a dialogue series filmed in the arena.
An episode goes from script to finished film in five steps, all driven by files
under `scripts/drama/episodes/<episode>/`:

| File          | Holds                                                                                      |
| ------------- | ------------------------------------------------------------------------------------------ |
| `lines.json`  | the cast (voice, speed), pronunciations, and every spoken line by scene                    |
| `beats.json`  | each scene as a run of lines, holds and named marks, with the gaps between them            |
| `edit.json`   | per scene: its stage, cameras, cuts, captions, and sound (beds, music, effects)            |

Stages, which are the staged performances the cameras film, live in
`tools/arena/promos/dead_company/stages/*.stage.json`. The arena loads them at
startup (`tools/arena/arena_drama_scenarios.cpp`), so moving an actor or
retiming a step needs no rebuild. `SOI_DRAMA_STAGES` points the arena at
another directory.

## Making an episode

```sh
EP=scripts/drama/episodes/ep01_killed_in_advance
OUT=artifacts/drama/ep01
QWEN=~/.local/share/soi-qwentts/bin/python   # torch cu128, qwen-tts, faster-whisper, speechbrain
CBX=~/.local/share/soi-cbx/bin/python        # torch cu128, chatterbox-tts installed --no-deps
$QWEN scripts/drama/perform.py $EP/lines.json --work $OUT/voicework --takes 3
$CBX  scripts/drama/convert.py $EP/lines.json --work $OUT/voicework
$QWEN scripts/drama/cast.py    $EP/lines.json --work $OUT/voicework --out $OUT/vo
python3 scripts/drama/plan.py $EP --vo $OUT/vo --out $OUT/plan --quality final   # or fast / preview
python3 scripts/drama/graphics.py $EP --out $OUT/graphics
scripts/promo-guarded-capture.py $OUT/plan/spec.json $OUT/clips --chunk 8
python3 scripts/drama/contact_sheet.py --plan $OUT/plan --clips $OUT/clips --out $OUT/sheets
~/.local/share/soi-tts/bin/python scripts/drama/assemble.py $EP --plan $OUT/plan \
    --clips $OUT/clips --vo $OUT/vo --graphics $OUT/graphics --out $OUT/ep01.mp4
```

1. **Voices are performed, then recast.**
   - `perform.py` asks Qwen3-TTS VoiceDesign (Apache-2.0) for several takes of every line. The instruction is the character's `voices` description plus the line's `direct` field ("sudden furious outrage, forgetting to whisper").
   - VoiceDesign acts well, but it invents a new voice on every call: speaker similarity between two calls was about 0.
   - `convert.py` runs each take through Chatterbox VC (MIT) into the character's designed reference voice. The delivery survives, and similarity rises to 0.5–0.8.
   - `cast.py` transcribes each take with faster-whisper, scores voice similarity with ECAPA and pitch spread, and keeps the best one.
   - Lines it flags (WER over 0.2) are usually spellings, numbers or Latin words; read the `heard` field before re-rendering.
   - Kokoro (`voice.py`) is kept for scratch reads only; it is flat.
2. **Timeline.** Every time in the edit is an expression resolved against the recorded lines: `12.5`, `line:s03_05`, `line:s03_05@end+0.3`, `mark:crack-1`, `end-2`.
   - Stage steps carry an `at` expression too. `plan.py` resolves it into the step's `t` and rewrites the stage file.
   - Re-recording the voices therefore moves walks, turns and reveals along with the cuts. Expressive voices made episode 1 run 1:20 longer.
3. **Gestures.** `plan.py` writes `<stage>.gestures.json` beside each stage.
   - Each line plays a talking gesture on its speaker: `talk` by default for lines of 1.6 s or more, or the line's `gesture` in `beats.json` (`point`, `shrug`, `nod`, `crossed`, `salute`, `chin`, `open`).
   - Gestures never overlap, and actors that already have a routine (sitting, kneeling) are left alone. A scene with `"gestures": false` stays still.
   - The arena turns the cues into a showcase routine with `wait:` gaps.
4. **Plan.**
   - Each scene is one continuous performance. Every camera the edit cuts to is captured once, over the span it is used plus 0.35 s handles.
   - Static cameras creep: dialogue framings push in (`push`, m/s), and wides (fov 36° and up) drift sideways (`drift`).
   - Framings narrower than 40° get handheld sway (`shake` degrees; `steady` turns it off).
   - A `track` camera rides a group with focus-space offsets, and `keys` give an explicit camera move.
   - Quality presets are scope 2.35:1: `final` is 1920×816 at 48 fps, `fast` is 24 fps, `preview` is 1280×544.
5. **Capture** with the guarded runner. It needs a GPU display, so suspend blanking for the run (`xset s off -dpms`) and restore it afterwards.
6. **Assemble.**
   - Picture:
     - Cuts frame-accurately from the cut list. A 48 fps capture is averaged in pairs down to 24 fps, which gives a 180° shutter blur.
     - Applies the scene's `grade` (an ffmpeg filter chain), then film grain and a vignette, and letterboxes to 1920×1080.
   - Sound:
     - Everything is mixed in stereo. Voices get a little `room` (`night`, `tent`, `open`, `camp`).
     - Every bed, music and effect file is first brought to dialogue level, so an edit's `db` means "this far under the voices". `cc0/...` paths come from the CC0 library in `artifacts/drama/cc0/lib` (see its `manifest.json` for sources and licences).
     - Beds duck 5 dB and music about 7 dB under speech. The mix is set to -16 LUFS by one static gain and a limiter.
   - Subtitles sit in the lower letterbox bar.

## Staging rules learned the hard way

- **Spawn positions are snapped to cell centres** that sit half a cell past the
  requested point. The loader asks half a cell short, so an authored `[x, z]`
  lands where it says. Walk destinations are not snapped.
- **A walker turns to its direction of travel.** To end facing someone, make the
  last leg of the walk point at them. `turn` (a centre wheel) rotates by a
  relative angle.
- **Civilian garments** (`builder_tunic_roman`, `carthage_robes`,
  `roman_civilian_mantle`, the Carthaginian civilian headwrap) render on a
  standing figure but vanish while it walks under the swordsman renderer.
  Dress the cast in `gallic_tunic`, `iberian_tunic` or `armor_light_carthage`.
- **Unarmed leads use the caster body** (`k_caster_humanoid_asset`). On the sword
  body they stand in a fists-up sword guard.
- **Headgear, not hair.** Primitive hair read badly; period headgear reads well.
  `palla_veil`, `pileus`, `turban` and `skull_face` are head attachments in
  `render/equipment/helmets/hair_bun.cpp`. In head-socket space the face is +z,
  the eyebrows sit near y -0.1 and the crown near y 1.35, about 1.4 units in
  radius. Anything skin-coloured reads as a bald head, so dye cloth strongly.
- **Props shove actors.** A unit spawned beside a prop is pushed off its tile,
  and a `supply_cart` at scale 0.9 is about 3 m long. Keep props well clear of
  the blocking and out of any eyeline you want to film.
- **Review every cut, not the posters.** `scripts/drama/contact_sheet.py` pulls
  one frame from the middle of each cut. Verify blocking from the
  `NN_shot.camera.jsonl` group positions and facings, not from a guess.
- **Redo shots without recapturing everything.** Capture a spec holding just the
  redone shots into `clips/zz_redoN`. The assembler takes the last match in path
  order, so a redo wins over the original take.
- **Grade a scene** with `"grade"` (an ffmpeg filter) on the scene in
  `edit.json`, for example to cut through river mist.
- **There is no lip-sync or head turn.** Gesture clips are showcase moves in
  `animation/showcase_pose_manifest.cpp` (clips 90–97). They are designed with
  `scripts/preview-showcase-pose.py` and checked after `bake_creature_assets`
  with `humanoid_preview --clip showcase_gesture_talk`. A full-body gesture
  overrides locomotion, so keep gestures off walking actors.

## The cast

Costumes are renderer keys under `troops/roman/dead_company/` and
`troops/carthage/dead_company/`, with loadouts in
`assets/visuals/unit_equipment_loadouts.json` and colours in
`render/humanoid/runtime/style_palette.cpp`. `troops/iron_sepulcher/skeleton_legionary`
is a skeleton in Roman kit. `skeleton_legionary_bare` drops the helmet and wears
`skull_face`, a cartoon skull, because the skeleton body's own head is a
featureless egg. In a stage, `look` takes a short name (`felix`,
`livia`, `tiro`, `severus`, `bostar`, `skeleton_legionary`) or any renderer key.
