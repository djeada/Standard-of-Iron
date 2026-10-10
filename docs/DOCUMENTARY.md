# Documentary post-production

_The Barcid Road: Hannibal's War_ (epic #1547) is finished by
`scripts/documentary`, a small Python package with one entry point:

```sh
python3 scripts/documentary <command> [EDIT] [--clips DIR] [--vertical-clips DIR]
                                       [--vo DIR] [--overlays DIR] [--work DIR]
```

It covers the narration bus (#1533), the series graphics (#1534), the 4K
YouTube package (#1535), the reel edit (#1536) and the score plan and stem
tools (#1537). It builds on the trailer pipeline rather than forking it: end
cards come from `promo-edit.py`'s `steam_demo` preset and its Steam-link
check, and delivery QC reuses `scripts/trailer/deliver.py`'s thresholds and
freeze detection.

Python needs only the standard library and Pillow (the system `python3` has
both); `ffmpeg`/`ffprobe` do all audio and video work. No numpy.

## The episode edit

An episode is one JSON file. `scripts/documentary/samples/ep07_cannae.json`
is a complete example wired to placeholder clips, with its script beside it.

| Key                                                           | What it holds                                                                                      |
| ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `schema`, `id`, `number`, `slug`, `title`                     | `soi-documentary-episode/1`, `ep07`, `7`, `cannae`, `Cannae`                                       |
| `place`, `region`, `date`                                     | the stamp: `{"day": 2, "month": "August", "year": 216, "era": "BC"}`                               |
| `script`                                                      | the narration script (beside the edit)                                                             |
| `summary`, `sources`, `credits`, `scale_note`, `next_episode` | description, end card tease                                                                        |
| `sides`, `strength_source`, `casualties`                      | order of battle, name plates, counters, tallies                                                    |
| `paths`                                                       | `clips`, `vertical_clips`, `vo`, `overlays`, `work` (CLI flags override)                           |
| `mix`                                                         | `duck_db` (-9 to -12), `attack`, `hold`, `release`, `merge_gap`, `lufs`, `true_peak`, `vo_lufs`... |
| `sections`                                                    | the episode template in order; each has `kind`, `title`, optional `chapter`, and `shots`           |
| `vo`                                                          | where each paragraph starts: `{"id": "p03", "at": "vo:p02@end+0.8"}`                               |
| `music`, `beds`, `sfx`                                        | cues with `at` and `dur`/`until`; music has a `role` and `plan_cue`                                |
| `graphics`                                                    | timed graphics events (types below)                                                                |
| `thumbnail`, `reel`, `takes`                                  | thumbnail title, the reel block, pinned takes                                                      |

A **shot** is a `clip` (`"capture/shot"`, the arena's `NN_shot.mp4`, or a
file), a full-frame `graphic`, or `black`, and lasts `dur` seconds or `until`
a time. Its `in` point is seconds or an arena timeline event
(`"event:first_contact-2.0"`, resolved from the capture's `timeline.json` and
the shot's `scene_start`). `overlays` lists pre-rendered RGBA layers —
`.mov`/`.webm`/`.mkv` with alpha, or a folder of PNG frames — with `opacity`,
`offset` and `in`; this is where the world-registered tactical overlays of
#1532 come in. Optional: `fade_in`, `fade_out`, `join: "dip"`, `shutter`,
`grade`, `game_audio_db` (lay the arena's recorded mix under the shot),
`thumbnail: true`.

**Times** are expressions, resolved against the real recordings:

| Expression                             | Meaning                                   |
| -------------------------------------- | ----------------------------------------- |
| `12.5`                                 | absolute seconds                          |
| `shot:ring+0.4`, `shot:ring@end-1`     | from a shot's start or end                |
| `section:battle`, `section:battle@end` | from a section's start or end             |
| `vo:p03+0.4`, `vo:p03@end+1.2`         | from a narration paragraph's start or end |
| `cue:battle_a+8`, `graphic:g_oob@end`  | from another cue or graphic               |
| `end-6`                                | from the end of the episode               |

Shots run back to back. Cutting a shot `until` a paragraph's end and placing
the next paragraph from that shot is how the picture is cut to the voice;
circular references are reported.

## Narration (#1533)

**Script.** Markdown, one numbered paragraph per recording:

```
## The ground
[ep07_p06] Early that summer Hannibal seized the Roman supply depot at Cannae...

## Reel
[ep07_r01] Eighty-six thousand Romans marched onto this field.
```

`ep<NN>_p<NN>` are episode paragraphs, `ep<NN>_r<NN>` the reel's lines.
`{stage directions}` and `<!-- notes -->` stay out of captions and counts.
`script EDIT` prints the word count (aim for 550-750) and which recordings
exist. Ids are fixed once recorded; never renumber.

**Recording guide.**

- One file per paragraph: `ep07_p03.wav`, 48 kHz, 24-bit, mono, recorded dry
  (no EQ, compression, reverb or noise reduction). Leave about a second of
  silence at each end; the tool trims to the speech.
- A retake is `ep07_p03_t2.wav`, `_t3`... The highest take wins; pin another
  with `"takes": {"p03": 2}` in the edit.
- Record 30 s of the room with nobody speaking as `ep07_roomtone.wav` once per
  session; it fills the gaps between paragraphs. Without it a quiet
  pink-noise floor is used.
- Peaks around -6 dBFS; same mic distance and position for every session.
- Scratch reads for the animatic go in `<vo>/scratch/`; they are used only
  where no final take exists, and delivery refuses them.

**Processing chain** (per take, `narration.CHAIN`): trim to speech
(0.12 s pre-roll, 0.3 s tail), 80 Hz high-pass, -2.5 dB at 220 Hz, +1.5 dB at
3.2 kHz, de-esser, 2.5:1 compression, then a gain to `mix.vo_lufs` so every
paragraph sits at the same level. Processed takes are cached by file size,
mtime and chain: drop in a new take and only it is reprocessed.

**Ducking.** Music sits `duck_db` (default -10 dB) under words, keyed by the
speech segments of every take. The duck starts `attack` (0.35 s) before the
first word, holds `hold` (0.2 s) after the last, and recovers over `release`
(0.9 s); pauses shorter than `merge_gap` (0.9 s) stay ducked. Ambience beds
duck `beds_duck_db` (-6), the arena's game audio `game_duck_db` (-4); sound
effects are never ducked. A music cue with `stems` keeps only its `under_vo`
stems under words and lifts the rest between lines.

**Subtitles.** `subtitles EDIT --out DIR` writes SRT and VTT from the script
text and the takes' timing: sentences are split, long ones broken at a comma
or conjunction, two lines of at most 42 characters, 1-7 s on screen. Sentence
boundaries snap to the pauses in the recording.

**Loudness.** The mix is gain-matched to -14 LUFS integrated and limited 0.6 dB
under the -1 dBTP ceiling (the AAC encode adds about that much). The report
(`<work>/mix/ep07.mix.json`) includes the VO's short-term median, its
p10-p90 spread (warning over 6 LU), paragraphs more than 2 LU off the median,
and how far the narration sits above the music (warning under 8 LU).

**Rebuilding.** Because every cue, shot, graphic and caption hangs off the
paragraphs, a new take re-times all of them on the next `timeline`,
`subtitles`, `mix` or `package`.

## Series graphics (#1534)

All graphics render from the edit's data in the game's display face
(`StandardIronDisplay-Bold.ttf`, falling back per glyph to EB Garamond for
`_`, `·` and lowercase), at any size, in 16:9 and 9:16 layouts.

| Type              | Shows                                                                              |
| ----------------- | ---------------------------------------------------------------------------------- |
| `title_sequence`  | the series title, rule and episode line over rising embers (<= 10 s)               |
| `place_date`      | `CANNAE · 2 AUGUST 216 BC` with the region beneath                                 |
| `order_of_battle` | both sides: commanders, totals counting up, breakdown, bars, source and scale note |
| `name_plate`      | a commander's name and office, in his side's colour (`commander` id)               |
| `army_counter`    | side totals ticking `values: {"rome": [0, 86000]}`                                 |
| `casualty_tally`  | losses per side, ranges and the authority for each figure                          |
| `chapter_card`    | numeral, rule, title (`section` id, or `numeral` + `title`)                        |
| `end_card`        | next-episode tease and the `steam_demo` CTA with the full store URL                |

`graphics EDIT --stills DIR` writes a still of every graphic in the edit at
1920x1080 and 1080x1920 (plus the reel's hook, caption and tag) over the
game's load-screen art, for review.

**The style module.** `scripts/documentary/style.py` is the single source of
colours (the game's ink/gold/iron from `ui/theme.h`; Roman red and
Carthaginian blue, with contingent shades), faces, type scale, tracking,
strokes, opacities, safe margins and timings. Sizes are authored at a
1080-pixel short side and scaled by `style.unit()`. Other tools import it:

```python
sys.path.insert(0, "scripts")
from documentary import style
style.side_color("carthage"), style.STROKE["arrow"], style.safe_box(3840, 2160)
```

Renderers outside Python read `python3 scripts/documentary style --json
tokens.json`. `style.qml_argb()` spells colours `#AARRGGBB` for QML.

## Delivery (#1535)

```sh
python3 scripts/documentary package EDIT --out DIR [--profile 2160p] [--codec h264|hevc]
```

One command builds the mix, conforms the picture, lays the graphics, encodes
and checks, and writes the upload package:

| File                         |                                                                                                                            |
| ---------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `ep07_cannae_2160p.mp4`      | 3840x2160, H.264 High CRF 15 (VBV 68 Mb/s) or H.265 CRF 17, BT.709, AAC 320k 48 kHz                                        |
| `ep07_cannae.en.srt`, `.vtt` | subtitles                                                                                                                  |
| `chapters.txt`               | YouTube timestamps from the sections (a section with `"chapter": false` folds into the previous one)                       |
| `thumbnail.jpg`              | 1280x720 title treatment over the best frame (< 2 MB)                                                                      |
| `thumbnail_candidates.jpg`   | every scored candidate, best first                                                                                         |
| `description.txt`            | summary, chapters, sources (Polybius, Livy), footage and scale notes, credits, AI-audio disclosure, Steam and GitHub links |
| `upload.json`, `mix.json`    | what was built, loudness and QC                                                                                            |

The conform reads the 2x supersampled captures and downsamples with Lanczos;
clips captured at a whole multiple of the frame rate get motion blur from
their sub-frames (`shutter`, default 180°). Shot segments are lossless and
cached, so a re-cut re-renders only the shots that changed.

Checks before a file is kept: frame size, picture/sound length, -14 ±1 LUFS,
true peak <= -1 dBTP, frame zero lit, no stray black or frozen frames outside
graphic shots and fades. A failing encode is left as `*.rejected.mp4`. The
package also refuses rule errors (below), YouTube-invalid chapters (fewer than
three, first not at 0:00, any under 10 s), and narration that is synthetic,
scratch or missing unless `--allow-scratch` (review packages only).

Profiles: `2160p` (delivery), `1080p`, `preview` (960x540, fast), `tiny`
(320x180 at 12 fps, for tests).

## Rules

`check EDIT` reports the series rules; errors block `package`:

- only `bed`, `tension` and `elegy` cues under narration; `theme`, `battle` and
  `climax` never under words; `duck_db` within -9..-12 dB;
- `"hit": true` effects and cue `hits` land between lines;
- each `climax` cue has `pre_silence` 0.5-1.5 s with no words or other music;
- frame zero is footage with no fade-in; the title sequence is <= 10 s;
- the episode ends on an `end_card` (Steam CTA);
- `combat_last_defensive_wall` is never used; reels warn on slow tracks;
- warnings: script outside 550-750 words, narration outside about half the
  runtime, runtime outside 5-8 min, cold open over 20 s.

## The reel (#1536)

```sh
python3 scripts/documentary reel EDIT --out DIR [--profile 1080p]
```

The edit's `reel` block lists six parts: `hook`, three `beat`s, `consequence`,
`tag`. Each voiced part plays one reel line (`ep07_r01`...) recorded and
processed like the episode's paragraphs and lasts the line plus `pad`. The
hook's claim is on screen at full opacity from frame zero; every later line
is captioned in big capitals above the platform UI; the tag card says "full
battle on the channel" and carries the Steam link. Pictures come from
`paths.vertical_clips` (the capture specs' vertical takes of the same
scenario passes). Delivery is 1080x1920, -14 LUFS, refused at 60 s or longer.
The music must be driving: never the banned track; slow titles are flagged.

## The score (#1537)

No music is generated here and no service is called.

```sh
python3 scripts/documentary score plan [--json]          # cue counts / full cue list
python3 scripts/documentary score manifest --out req.json  # generation requests
python3 scripts/documentary score normalize-stems STEMS --out NORMALISED
python3 scripts/documentary score validate-stems NORMALISED
python3 scripts/documentary score check-reuse --edits ep01.json ... ep09.json
```

`data/score_plan.json` holds the music rules, the three themes, each battle's
palette (key, tempo, colour, instrumentation, library references) and the cue
template. Expanded, it is **111 cues, about 82 minutes, 363 stems**: per
episode a cold-open tension cue, two beds, ground and battle tension, three
battle cues (four for the Long War), a Kingdom of Iron climax variation, an
elegy, an end sting and a reel cut, plus the 10 s series title ident (the only
passage every episode repeats) and the trailer cue. That is enough fresh
material that no 60 s passage is used twice in the series; `check-reuse`
enforces it across edits.

`score manifest` writes one request per cue (Cannae first): prompt,
negative prompt, length, tempo, key, stems wanted, reference library tracks,
naming and loudness target, ready to feed to ElevenLabs Music by hand.

Stems come back as `<cue>/<cue>.<stem>.wav` (percussion, strings, brass,
choir; 48 kHz, 24-bit) with `<cue>/stems.json` per `data/stem_schema.json`
(tempo, key, duration, stems, source render name, date, licence).
`normalize-stems` applies one common gain so the stems' sum sits at -16 LUFS
without changing their balance (backing off to keep -1 dBTP); `validate-stems`
checks the metadata against the plan, the files against each other and the
measured levels. An edit uses stems as `"stems": {"strings": ..., ...}` with
`"under_vo": ["strings"]`.

**Provenance after generation:**

1. Keep the service's render name and the manifest's `request_id` in each
   `stems.json`.
2. Record the batch in `scripts/documentary/data/score_provenance.json`
   (render name → cue, date, licence), mirroring `BATCHES` in
   `tools/audio_import/import_music.py`.
3. Stems stay outside the repository; they are not game assets.
4. Only if a cue also ships in the game: import it with
   `tools/audio_import/import_music.py`, give it a provenance block in
   `assets/audio/audio_manifest.json`, re-render `docs/AUDIO_LICENSES.md`
   with `scripts/audio_provenance.py --doc`, and update the Music section of
   `THIRD_PARTY_LICENSES.md`.
5. Every episode description carries the AI-audio disclosure (the package
   writes it).

## Trying it without any footage

```sh
EDIT=scripts/documentary/samples/ep07_cannae.json
D=artifacts/documentary/ep07
python3 scripts/documentary synth-vo $EDIT --vo $D/vo
python3 scripts/documentary placeholders $EDIT --clips $D/clips --vo $D/vo --overlays $D/overlays --work $D/work
python3 scripts/documentary check $EDIT --clips $D/clips --vo $D/vo --work $D/work
python3 scripts/documentary package $EDIT --clips $D/clips --vo $D/vo --overlays $D/overlays \
    --work $D/work --profile tiny --out $D/package --allow-scratch
python3 scripts/documentary reel $EDIT --clips $D/clips --vo $D/vo --work $D/work --profile tiny --out $D/reel
```

`synth-vo` writes stand-in narration (espeak-ng when installed, otherwise
voiced tone bursts with sentence pauses) and marks the folder `.synthetic`;
`placeholders` writes arena-shaped captures (`NN_shot.mp4`, `shots.json`,
`timeline.json`) at twice the frame rate and RGBA arrow overlays. Neither is
ever delivered.

Tests: `tests/scripts/test_documentary_edit.py` (timing, retakes, captions,
rules, ducking envelope, style, score plan; standard library only) and
`tests/scripts/test_documentary_pipeline.py` (the whole chain at `tiny`, the
measured duck depth, stem normalisation, graphics in both aspects; skipped
without ffmpeg and Pillow, about two minutes).
