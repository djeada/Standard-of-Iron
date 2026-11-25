# Playtest plan: polish, play big soon, asymmetric nations

Status: started 2026-11-24. Source: two hand-played sessions on a fresh build
(tutorial won; Crossing the Rhone lost; Two Fords and Amber Delta skirmishes lost
to Normal AI; Frostmere Basin observed). Every item below was seen on screen,
not inferred from code. Owners, by difficulty:
- **O** = Opus orchestrator: plan, review, integration, commits, final playtest.
- **S1, S3, S4, S5** = Sonnet agents: straightforward fixes, one stream each.
- **T, D, N** = Opus agents: tutorial and AI behaviour (T), play-big-soon economy (D),
  nation asymmetry (N).
- **F1, F2** = Fable agents: the big beauty work - terrain and maps (F1),
  unit art and animation (F2).

The previous commander-RPG plan (64 open items) lives in git:
`git show 9c835e51a:todo.md`. It is paused, not abandoned.

## Working rules

- One shared working tree, no worktrees, never stash. Another session may be
  editing; touch only the files your item needs and never revert unrelated diffs.
- Builds go through the shared lock only (one build at a time, `--parallel 3`;
  the CPU runs hot). Prefer the per-TU syntax checker for C++ edits. A failure in
  a file you do not own is another stream mid-edit: wait and retry, never touch it.
- Render and play off-screen only (Xvfb + VirtualGL), each stream on its own
  display; never open windows on the user's screen; games launch muted.
- Commits: O commits per stream after review. No AI attribution in messages.
  Do not push or open PRs without the user's go-ahead.
- Fix the root cause. Never change a test expectation to match a bug.
- Every closed item names its evidence: a test, a screenshot, or a log line.
- Graphics quality is decided once at first launch, then only the player changes
  it. Shadows, selection rings and floating numbers are essential; polish them,
  never remove them. Formation melee interpenetration is intended.

## Pillars these items serve

1. **Readability:** units, roles, terrain and threats read at a glance.
2. **Every input answers:** no silent refusals, nothing stale while paused.
3. **Play big soon:** the player grows a city and an army as fast as the AI does.
4. **Asymmetric nations:** Rome and Carthage play differently, not ±10% apart.
5. **Beauty:** naturalistic large-scale terrain (see the inspiration screenshot:
   varied ground patches, full trees, aerial haze, thousands of small soldiers).

## P0 - bugs

- [x] **B1 Graphics dropped mid-match** - O. Preset fixed once at first launch,
      battle-time governor removed. Evidence: fresh-profile log line
      "first launch, keeping the high graphics preset as the player's choice".
- [x] **B2 Stale while paused** - S1. Placement ghost freezes / never appears when
      entered paused; recruit queue and reserve do not update until resume.
- [x] **B3 Tutorial AI too strong** - T. "easy/defensive" outpost builds a town,
      fights during economy steps, trains 320. It should hold its camp and only
      field what the tutorial script needs.
- [x] **B4 Key conflict R** - S4. "Tilt camera up" secondary and Rally share R;
      Rally wins with a commander selected. Remove the clash and make Settings
      flag conflicting bindings.
- [x] **B5 Battle stats disagree** - S3. Log "Losses: 1" vs report 50; enemy
      credited 1 kill; spectator panel shows losses but zero kills for everyone.
- [x] **B6 Phantom report rows** - S3. Iron Sepulcher listed as a routed army with
      4 barracks and 2000 score; duplicate "Roman Republic (3)" row on Rhone.
- [x] **B7 Spectator addressed as owner** - S3. "The cursed vein is yours..."
      shown while observing.
- [x] **B8 Tutorial "Show me" targets** - T. Step 5 points at an empty map edge,
      final step at the player's own base.
- [ ] **B9 Tutorial step 2 skipped** - T. Continue on step 1 jumped to step 3
      once without a move order; reproduce in a headless tutorial test first.
      Reopened 2026-11-25: S1 saw Continue on step 1 jump to step 3 again after T's
      Skip debounce.
- [x] **B10 Nation change recolours the player** - S4. Blue -> Orange silently.
- [x] **B11 Stale recruit panel after defeat** - S1. Shows a barracks the player
      no longer owns.
- [x] **B12 Builder click in the barracks yard selects the barracks** - S4.
- [x] **B13 Skirmish map list** - S5. ~60 s spinner on first open; stream entries
      in as they load or cache the metadata.

## P0 - regressions found during the work

- [x] Observed/skirmish match segfaults ~2 min in, at the first fight: render thread,
      RiggedCullPipeline::draw_full_mesh_pass in directional shadows, after "Forbidden
      render-time bake: skin palette UBO missing for rigged mesh entry" (cores 98569,
      434127). Suspect: units never prewarmed (mercenary bands of a foreign nation?) - N.

- [x] Main menu > Tutorial (and briefing missions) freezes behind the paused
      briefing; --skip-briefing works. Likely the paused-match pass from B2 - S1.
- [x] Tutorial card goes stale while paused (hint, Show me) - S1.

## P1 - interface and feedback

- [x] Build menu greys out while builders build, with no reason; allow queuing
      several sites (Shift) and explain refusals - S1.
- [x] Neutral barracks card says only "Neutral"; say it can be captured and how - S1.
- [x] Skirmish setup CPU row lacks "Nation"/"Commander" chip labels - S4.
- [x] Spectator panel truncates names ("Quintus Fabius ...") - S4.
- [x] Defeat wording: "MISSION FAILED / Retry mission" vs "ARMY BROKEN / Fight it
      again" for the same skirmish result; campaign report says "Player 1" - S3.
- [x] Marcellus' scout line arrives ~10 min late; camera hint card pops behind the
      victory dialog - T.
- [x] Rhone briefing: "take the second camp" over a "0 of 2" counter - T.
- [x] Main menu promises "fleets"; there are none - T (fix the copy).
- [x] Load game / Field Manual: tiny text in a mostly empty frame; manual is a
      9-tab wall of text - S5.
- [x] Idle-builder hotkey and visible control-group strip - S1.

## P1 - play big soon (design first, then build)

Observed: Rome start buys exactly two squads (60 iron); at 7 min the AI had
trained 527 to the player's 36. The AI grows big quickly; the player cannot.

- [x] **D1 design** `docs/design/PLAY_BIG_SOON.md` - decided 2026-11-24 (see its Decisions).
- [x] Changes 1, 2, 3, 6 (civilians auto-deliver, camp start, iron 45/200, home cap 6,
      map-authored gold 500) - D.
- [x] Changes 4, 5, 7, 9 (builders resume work, Shift=5/Ctrl=repeat/queue 10, teach the
      opening, several sites per crew) - S1 after its P0/P1 items.
- [ ] Change 8 (caps to 2,000 or 1,500 after the benchmark) - O.
- [x] Changes 10-11 (late gold sink, forward depot) - N, with the asymmetry work.
- [ ] Change 12 (visible city growth) - F1 after the terrain pass.
- [x] Change 13 ("trained" counts fighting men only) - S3 (B5).

## P1 - asymmetric nations (design first, then build)

Observed: Rome and Carthage share every building and 12 core units with stats
within ~10%; only elephants/slingers vs velites and shield wall vs testudo differ.

- [x] **D2 design** `docs/design/NATION_ASYMMETRY.md` - decided 2026-11-24.
- [x] Phases 0-3 (spikes, data identity, Carthage gold army, Rome levy army) - N;
      new building art for phases 2-3 - F1/F2 after their current passes.
- [ ] Phase 4 (roads, harbour, commander traits, dark layer) - N.
- [ ] Phase 5 balance matrix - O with N. Now Rome 56% / Carthage 44% (18 matches).
- [ ] AI waves attack a full garrison early and lose the army (Hanno 5/6 vs Rome) - T.

## P1 - visuals (F1 terrain and maps, F2 units and animation)

Parked until the Fable limit resets (2026-11-29 16:00): F1 broadleaf trees (second
attempt unseen), hedgerows (farm_hedgerow started), change 12; F2 follow-ups below.


Terrain toward the inspiration screenshot:
- [x] Snow reads as sand: wavy ripple pattern everywhere; needs a snow material.
- [x] Cliffs are stretched striped grass; rock/cliff material.
- [x] Teal-black cliff shadows on Two Fords / Amber Delta; grade.
- [x] Grass blades knee-high next to soldiers, poking through feet and rings.
- [x] Ground variety: more natural patches, less tiling; aerial haze, long views.
- [x] Untextured green mountains at map edges; faint seam in the sky.
- [x] Hills render as steep blue-white mesas on Frostmere.

Units:
- [ ] Roman builder HUD icon still shows a crested legionary (assets/visuals/icons/builder_rome.png) - F2.
- [ ] Still close at RTS zoom: Roman archer vs legionary; Roman vs Carthaginian cavalry - F2.
- [x] render_tests: EveryBuildingKindDrawsCatalogDescriptions fails, carthage/castra draws nothing - N.
- [x] Builders look exactly like legionaries (crest, armour, no tools).
- [x] Bodies are one beige mannequin colour; separate skin, cloth, metal, simple faces.
- [x] Possible pop-out of a soldier group between frames in the Frostmere fight;
      capture cleanly before fixing.

Maps:
- [ ] Amber Delta: player barracks yard overhangs the cliff edge.
- [ ] Sepulcher shrine fog blob has a hard edge; loose black debris around it.
- [x] Tan blob on the Rhone river near the start.
- [x] Roman temple about twice the barracks' size.

## P2 - difficulty and pacing

- [ ] Carthage missions after asymmetry: the playthrough bot no longer takes the Rhone hill
      fort (22/155 men left) and loses Trebia at 7:53. Play both by hand - O.

- [ ] Rhone mission I: camera opens on an empty field with a wave 30 s out; the
      first hill fort erases a 170-man army - T (camera and balance), O reviews.
      Done: opens on the army with --skip-briefing; two of four hill-fort towers removed.
      Open: briefing path blocked by the menu freeze below.
- [x] Tutorial scout fight costs 23 of 88 soldiers - T with B3.
- Normal skirmish strength stays as is for now (user ruling 2026-11-24).

## P2 - audio

- [x] Menu music is a ~55 s loop repeating every minute; vary or lengthen - S5.
- [x] Mix is quiet: ~-30 dB RMS, -7 dB peaks at default master 70% - S5 measures
      against the game's null sink before changing levels.
      Measured: menu -30 LUFS at defaults; music/ambience sit 10-14 dB under
      voice/effects by the default sliders. No mastering change needed.

## Not yet playtested (next session)

Save/load mid-match, German UI, Carthage close-ups, elephants and siege engines,
campaign missions II+, unit silhouettes on sand and in shadow.
