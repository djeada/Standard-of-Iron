# Architecture and optimization backlog

Source audit: 2026-09-05, commit `4ee43d45`.

This is a prioritized static review of the simulation, navigation, AI, renderer,
animation/assets, application/QML, persistence, audio, build, and verification
paths. Code mechanisms below were inspected; runtime failures and performance
improvements have not been reproduced or measured in this audit. Existing
benchmark numbers are historical baselines, not measurements of this checkout.

P0 = correctness risk in synchronization or state capture; P1 = major runtime or
ownership problem; P2 = substantial maintainability, memory, or verification gap.
Each unchecked item describes remaining work, its evidence, and a completion
criterion. These are the major findings from the scan, not a claim that every
possible defect has been discovered.

## P0 — Correctness before optimization

- [x] **01. Make world replacement wait for actual reader/writer quiescence.**
      `GameEngine::WorldFreeze` waits up to two seconds, then explicitly logs
      "rebuilding the world anyway" even if rendering or simulation remains active.
      Its callers can subsequently reset session and renderer state. A slow frame
      therefore defeats the ownership guarantee intended to protect world teardown.
      Replace the timeout-and-continue behavior with an acknowledged stop/barrier;
      a timeout must leave the existing world intact or report a failed transition.
      Verify reload, shutdown, and load cancellation with a deliberately stalled
      render/simulation thread.
      Evidence: [game_engine.cpp](app/core/game_engine.cpp), `WorldFreeze`,
      `load_game_from_slot`, and `start_skirmish_internal`'s freeze lifetime.

- [x] **02. Capture saves at one simulation tick, including session metadata.**
      `begin_save` reads clock/RNG/economy, mission state, and world data in separate
      operations without acquiring the frame mutex or freezing simulation. Autosave
      reaches it directly from a GUI timer. `serialize_world` locks entity iteration,
      but reads registries and terrain afterward; this is not an atomic match
      snapshot. Saves can mix state from different ticks, and unsynchronized reads
      of mutable metadata need a concurrency audit. Queue a capture on the simulation
      thread, producing one immutable save snapshot before handing it to storage.
      Verify active-battle autosave and save/load continuation under thread checking.
      Evidence: [game_engine.cpp](app/core/game_engine.cpp), `begin_save`/`autosave`;
      [save_load_coordinator.cpp](app/persistence/save_load_coordinator.cpp),
      `begin_save_to_slot`; [serialization.cpp](game/save/serialization.cpp),
      `serialize_world`.

- [x] **03. Do not recycle GPU buffer storage after an unsuccessful fence wait.**
      `PersistentRingBuffer::begin_frame` ignores the result of a one-second
      `glClientWaitSync`, deletes the fence, and makes the region writable. A timeout
      or wait failure does not establish that previous GPU reads have finished.
      Handle every wait result and retain the fence/storage until reuse is safe;
      define a recoverable device/error path. Also handle `GL_WAIT_FAILED` explicitly
      in the backend frame-slot wait. Verify timeout and failure branches with a
      controlled GL test seam, as ordinary fast-GPU tests will miss them.
      Evidence: [persistent_buffer.h](render/gl/persistent_buffer.h), `begin_frame`;
      [backend.cpp](render/gl/backend.cpp), `wait_for_frame_slot`.

## P1 — Simulation ownership and scalability

- [ ] **04. Remove the shared frame mutex from normal presentation and input.**
      Simulation holds `m_frame_mutex` through `simulate`; presentation uses the same
      mutex for minimap updates, camera/commander work, and numerous UI synchronizers.
      After deferring presentation it eventually blocks. Input also acquires the
      frame lock, and effect rendering can skip submission when its lock budget
      expires. Separate immutable UI/effect snapshots and input messages from mutable
      simulation state. Measure input latency, lock wait, skipped presentation, and
      p95 frame time during a large battle; moving simulation to a thread has not
      eliminated these dependencies.
      Progress (2026-09-06): the contention is now measured rather than guessed --
      `lock_frame` counts uncontended and contended acquisitions, total and longest
      wait, and `update_presentation` counts deferred frames and forced waits
      (`GameEngine::frame_lock_stats()`). The lock itself is unchanged: 127 call
      sites across the view models still take it, and separating them needs the
      immutable UI snapshot this item asks for. Not done.
      Evidence: [game_engine.cpp](app/core/game_engine.cpp), `run_simulation_thread`,
      `update_presentation`, `render`;
      [client_context.h](app/core/client_context.h), `ClientHost::lock_frame`.

- [ ] **05. Replace broad ECS render-world copying with a versioned presentation payload.**
      Every requested snapshot walks the registry's slot high-water mark and checks
      dozens of component types. Active entities copy gameplay state including
      movement, production, combat, and vector-backed formation data. Idle signature
      reuse already exists, but active battles largely bypass it; a retained reader
      can also force allocation of a fresh `World` when both buffers are busy.
      Publish only render-consumed fields, share immutable layouts, track component
      revisions, and define bounded buffer/back-pressure behavior. Measure publication
      time, copied bytes, allocations, and retained memory with moving armies and a
      deliberately slow renderer.
      Progress (2026-09-06): the vector-backed presentation components
      (`FormationPresentation`, `FormationRosterPresentation`,
      `FormationHitPresentation`) are copied only when their content revision moves,
      so a squad that is moving or fighting no longer re-copies its whole soldier
      layout every frame. Buffers are a fixed ring of three with real back-pressure:
      when every buffer is retained the publication is skipped and counted instead of
      allocating another `World`. `RenderPublicationStats` reports publications,
      skips, entities copied and reused, and layouts skipped. Still outstanding:
      publishing only render-consumed fields rather than the whole component set.
      Evidence: [world.cpp](game/core/world.cpp), `copy_authoritative_snapshot_components`,
      `copy_presentation_snapshot_components`, `publish_render_snapshot`.

- [x] **06. Make navigation and its invalidation hooks belong to a session.**
      `NavGrid::s_pathfinder` is a process-wide `unique_ptr`; initialization replaces
      it, clears gate blockers, and installs static collision callbacks. A second
      match cannot have independent navigation simply by constructing another
      `SessionContext`. The pathfinder also resolves collision state through ambient
      accessors. Move the grid, gate state, and hooks into session-owned navigation
      services and inject terrain/collision dependencies. Verify two different maps
      stepped interleaved and concurrently without either initialization changing the
      other's paths or blockers.
      Evidence: [nav_grid.cpp](game/systems/nav_grid.cpp), `initialize`;
      [nav_grid.h](game/systems/nav_grid.h);
      [pathfinding.cpp](game/systems/pathfinding.cpp).

- [ ] **07. Finish removing ambient match lookup and process-bound service state.**
      The current checker reports 58 ambient call sites. `services_for` still falls
      back to ambient state for an unbound world, and the process binding is a plain
      global pointer. This can hide missing ownership and complicates parallel tests,
      replay worlds, and session teardown. Inject narrow services, make unexpected
      unbound-world access explicit, and give service bindings a safe lifetime.
      Cover accessors outside the checker's named list as well, such as
      `ArmyFormationRegistry::instance()` in serialization. Finish by testing two
      sessions and shrinking the ambient budget to zero, not merely renaming calls.
      Progress (2026-09-06): `ArmyFormationRegistry` was a function-local process
      static; it is now owned by `SessionContext` and reachable per world through
      `ArmyFormationRegistry::for_world`. Ten of its twelve call sites take a world
      or a session, and the checker now counts the accessor, so the pattern is
      covered rather than invisible. `services_for` still falls back to the ambient
      session for an unbound world, but the fallback is counted
      (`unbound_world_lookups()`) and fatal under `SOI_STRICT_WORLD_BINDING=1`.
      The budget is still 58: what is left are per-entity helpers handed an
      `Entity&` and no world, which need a service threaded from their entry points.
      Evidence: [ambient_session.cpp](game/core/ambient_session.cpp);
      [ambient_instance_budget.json](scripts/ambient_instance_budget.json);
      [check-ambient-instances.py](scripts/check-ambient-instances.py);
      [serialization.cpp](game/save/serialization.cpp).

- [x] **08. Put mission spawns, rewards, and progression on the fixed tick.**
      `simulate` advances mission waves/stages with the variable wall delta times the
      time scale before consuming fixed simulation steps. `MissionWaveRuntime`
      accumulates that delta and spawns units; the engine also grants rewards there.
      This creates an authoritative time base outside `SimulationClock`, especially
      when catch-up ticks are dropped. Move these operations into session runtime
      systems driven by consumed ticks, with announcements emitted as presentation
      events. Verify identical wave/reward ticks across different frame cadences,
      overload, pause, speed changes, and campaign replay.
      Evidence: [game_engine.cpp](app/core/game_engine.cpp), `simulate`,
      `update_mission_waves`, `update_mission_stages`;
      [mission_wave_runtime.cpp](app/mission/mission_wave_runtime.cpp), `advance`.

- [ ] **09. Make commander control reproducible through simulation input records.**
      Commander motor/action state is driven from app controllers, with direct
      component writes and explicit exceptions to the command-boundary checker.
      Recording accepted RTS commands does not capture this entire input mode.
      Define a tick-stamped commander input record consumed by a simulation-side
      controller; keep mouse capture, camera movement, and UI feedback in the app.
      Verify a replay containing movement, jump/dodge, attacks, target changes, and
      enter/exit transitions. This is required for complete gameplay replay and a
      renderer-independent host, even though ordinary RTS orders already use a queue.
      Progress (2026-09-06): `Game::Command::CommanderInputFrame` is the tick-stamped
      record -- every button, the view yaw and the dodge direction -- written to the
      replay at the tick the controller applies it (`replay_format` 3) and read back
      from the replay in place of live input during playback. Commander mode is
      therefore inside the replay contract. Still outstanding: the controller itself
      lives in `app/`, so a renderer-independent host cannot yet drive it.
      Evidence: [commander_control_controller.cpp](app/commander/commander_control_controller.cpp);
      [commander_mode_coordinator.cpp](app/commander/commander_mode_coordinator.cpp);
      [check-command-boundary.py](scripts/check-command-boundary.py).

- [x] **10. Fix formation-cache identity and lifetime before expanding caching.**
      Layout, spatial-layout, slot, and contact caches are `thread_local` maps keyed
      by raw `Entity*` or pairs of pointers. Their signatures lack a world/session
      identity and content revision; address reuse across worlds can satisfy the
      same signature while the applicable formation data differs. Whole-cache clears
      at 8,192/65,536 entries also create abrupt rebuild work. Bind caches to a world
      lifetime and generational entity identity, include content/layout revisions,
      and use bounded incremental eviction. Test world destruction/recreation with
      identical entity values and different formation definitions.
      Evidence: [formation_combat_geometry.cpp](game/systems/formation_combat_geometry.cpp),
      `g_layout_cache`, `g_spatial_layout_cache`, `g_contact_cache`, and prune helpers.

- [x] **11. Reduce formation-contact pair work with exact spatial pruning.**
      A contact-cache miss still sorts attacker slots and can compare every attacker
      slot against every target slot. Existing bounding checks and revision caches
      help, but moving/contact-changing formations invalidate results, and the worst
      case remains quadratic in soldiers per pair. Profile current cache hit rates
      and candidate counts; evaluate a slot spatial index or stronger exact bounds.
      Preserve contact geometry and combat outcomes with differential tests. Do not
      assume the older documentation's removed-cache implementation is still current.
      Evidence: [formation_combat_geometry.cpp](game/systems/formation_combat_geometry.cpp),
      `accumulate_slot_contact`, `resolve_contact`;
      [sim_budgets.json](tests/perf/sim_budgets.json) records historical simulation costs
      already exceeding a 16.67 ms tick budget at its large-unit fixtures.

- [x] **12. Eliminate repeated all-pairs work in AI snapshot construction.**
      Each AI snapshot scans resource props, collects friendlies/enemies, tests enemy
      visibility against friendly vision sources, then checks each friendly against
      visible enemies for engagement. Snapshot building happens on the simulation
      thread for each due AI, before the worker receives its job. This can scale as
      enemies × vision sources plus friendlies × visible enemies per AI. Reuse
      immutable resource/catalog data and compute visibility/engagement with spatial
      queries or a per-owner indexed snapshot. Verify equivalent information access
      and benchmark simultaneous decisions for seven growing AI economies.
      Evidence: [ai_snapshot_builder.cpp](game/systems/ai_system/ai_snapshot_builder.cpp),
      `build`, `is_visible_to_sources`;
      [ai_system.cpp](game/systems/ai_system.cpp), `update`.

- [x] **13. Bound deterministic AI decision work without blocking the whole tick indefinitely.**
      At a decision's due update, `AISystem::process_results` calls `wait_idle()`;
      that wait has no deadline. A slow AI job stalls simulation while it holds the
      world/frame locks. Each AI also owns a dedicated thread, independently of the
      renderer's preparation workers. Introduce measured work budgets and resumable
      deterministic jobs or a bounded worker scheduler. Preserve fixed application
      ticks: applying results whenever they happen to finish would break replay.
      Verify overloaded-worker behavior and measure worst-case decision wait time.
      Evidence: [ai_system.cpp](game/systems/ai_system.cpp), `process_results`;
      [ai_worker.cpp](game/systems/ai_system/ai_worker.cpp), `wait_idle`, constructor.

- [x] **14. Stop globally invalidating navigation caches for local changes.**
      A navigation revision change clears the entire path cache, and reaching 256
      cached paths clears it again. Connectivity maps rebuild when the global
      revision changes. Thus local construction/gate/topology changes can discard
      useful routes far away and move rebuild work into the next query. Introduce
      bounded replacement and affected-region dependency tracking where justified;
      instrument invalidations, rebuild cells, hits, and latency. Verify distant
      routes survive unrelated edits while routes through changed obstacles remain
      correct. Existing local dirty-region updates should be retained.
      Evidence: [pathfinding.cpp](game/systems/pathfinding.cpp), `update_navigation_grid`,
      `find_path`, and revision checks in region lookup;
      [pathfinding.h](game/systems/pathfinding.h), path/region caches.

- [ ] **15. Complete the execution model behind declared simulation phases.**
      `plan_phase_schedule` computes compatible batches, but `World::update` still
      invokes every system serially under the world lock. The default access contract
      is exclusive, and structural mutations in service/factory calls remain a
      barrier to useful concurrency. Establish complete component and service access
      footprints and phase-safe deferred mutations first; then parallelize only
      measured independent work with deterministic merges. Completion means replay
      equivalence, race checks, and a measured scaling benefit, not merely dispatching
      existing systems onto threads.
      Progress (2026-09-06): the prerequisite this item names first -- complete access
      footprints -- is now checkable. `Registry` records the component types a system
      actually touches (compiled in for Debug, out for Release, or forced with
      `SOI_VERIFY_SYSTEM_ACCESS`), `World::verify_system_access` compares that against
      the system's declaration, and a test runs a real tick and fails on any system
      that reaches a component it does not declare. No system is dispatched onto a
      thread yet, and should not be until that check is green on a full match.
      Evidence: [world.cpp](game/core/world.cpp), `plan_phase_schedule`, `update`;
      [system.h](game/core/system.h), `access`;
      [system_schedule.h](game/core/system_schedule.h).

## P1 — Rendering throughput and latency

- [ ] **16. Reduce repeated full-detail skinning and shadow GPU work.**
      Color and directional-shadow vertex shaders independently skin the same full
      indexed creature geometry. Instancing, rigid/blended ranges, shared bodies,
      resident BPAT data, and parallel preparation already exist; they do not remove
      repeated vertex work across passes/cascades. Measure current per-pass GPU cost,
      then evaluate exact pose reuse/pre-skinning and packed vertex/palette formats.
      Preserve the repo's full-LOD geometry, equipment, and shadow requirements in
      comparisons; publish warmed p50/p95 and memory costs on named hardware.
      Progress (2026-09-06): the far shadow cascades -- the ones already rendered into
      a lower-resolution texture -- now skin with the primary bone only
      (`rg_clip_position_rigid`) instead of blending up to four palette matrices per
      vertex, in both the GPU-driven and the full-mesh shadow paths. Not measured:
      the completion criterion asks for warmed p50/p95 on named hardware, and this
      machine cannot produce a trustworthy GPU timing.
      Evidence: [character_skinned_gpu_instanced.vert](assets/shaders/character_skinned_gpu_instanced.vert);
      [directional_shadow_rigged_gpu_instanced.vert](assets/shaders/directional_shadow_rigged_gpu_instanced.vert);
      [MASSED_BATTLE_PERFORMANCE.md](docs/MASSED_BATTLE_PERFORMANCE.md), historical
      GPU/back-pressure profiles and remaining-bottleneck discussion.

- [ ] **17. Optimize and benchmark the portable rigged-rendering fallback separately.**
      The per-command rigged pipeline sets uniforms, may blend/pack a palette on the
      CPU, uploads it to offset zero of one UBO, and issues `mesh->draw()`. When the
      fast batching path is unavailable or rejects a command, this carries per-draw
      CPU/driver work and possible buffer-update stalls. Add compatible instancing
      and immutable palette streaming where supported. Compare GL 3.3/4.1 with the
      fast path at equivalent quality, including body/equipment and shadow passes;
      fast-path measurements alone do not characterize supported fallback hardware.
      Progress (2026-09-06): the fallback wrote every palette to offset zero of one
      UBO, so consecutive draws serialised on the same range. It now streams through a
      64-slot ring aligned to `GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT` and orphans the
      buffer on wrap, which is the immutable palette streaming this item asks for and
      is portable to GL 3.3. Not measured against the fast path on fallback hardware.
      Evidence: [rigged_character_pipeline.cpp](render/gl/backend/rigged_character_pipeline.cpp),
      `pack_cmd_palette` and draw submission;
      [backend.cpp](render/gl/backend.cpp), rigged shadow fallback.

## P2 — Memory, maintainability, and verification

- [x] **18. Add byte-based residency limits for creature render assets.**
      `RiggedMeshCache` retains entries, base meshes, attachment meshes, and skin
      atlases until an explicit whole-cache clear. It exposes counts and upload
      counters but no residency budget or incremental eviction. Shared geometry
      already saves memory, yet broad prewarm and increasing visual diversity can
      still raise peak CPU/GPU memory. Track ownership and resident bytes, scope
      prewarm to required assets, and evict unused resources safely after GPU use.
      Verify a sequence of missions/rosters and record memory high-water marks;
      retained caching by itself is not evidence of a leak.
      Evidence: [rigged_mesh_cache.h](render/rigged_mesh_cache.h);
      [rigged_mesh_cache.cpp](render/rigged_mesh_cache.cpp);
      [creature_asset_prewarmer.cpp](render/creature/assets/creature_asset_prewarmer.cpp).

- [ ] **19. Stream long audio instead of fully materializing every track.**
      Decode reads the compressed file, accumulates the entire float PCM track,
      resamples/masters it, then allocates the retained int16 PCM copy. Worker decoding
      and mission unload policies already exist, but long music/ambience still pays
      full-track memory and overlapping conversion buffers. Separate streaming music
      and long ambience from short resident effects; set byte budgets and preserve
      the existing mastering, looping, and callback constraints. Measure peak memory,
      first-play latency, and underruns across mission transitions.
      Progress (2026-09-06): resident decoded bytes, a peak high-water mark and a
      byte budget (`SOI_AUDIO_PCM_BUDGET_MB`, 192 MB default) are tracked and
      reported, and the compressed source buffer is released as soon as the decoder
      stops referencing it. The track is still fully materialised: mastering
      analysis and loop sealing both need the whole track, so streaming playback
      needs those made incremental first. Not done.
      Evidence: [miniaudio_backend.cpp](game/audio/miniaudio_backend.cpp),
      `decode_into_slot` and PCM conversion;
      [miniaudio_backend.h](game/audio/miniaudio_backend.h), track storage.

- [x] **20. Move expensive save encoding off the GUI thread after coherent capture.**
      `begin_save_to_slot` constructs the complete world/terrain JSON document before
      `SaveLoadService::begin_save` queues background storage. Therefore asynchronous
      database writing does not remove capture/JSON stalls. After item 02 establishes
      a coherent immutable snapshot, encode/compress/write it in the worker and keep
      cancellation/progress meaningful during that work. Measure capture, encoding,
      peak memory, and longest GUI stall separately on a large late-game save.
      Evidence: [save_load_coordinator.cpp](app/persistence/save_load_coordinator.cpp),
      `begin_save_to_slot`; [serialization.cpp](game/save/serialization.cpp);
      [save_load_service.cpp](game/systems/save_load_service.cpp), `begin_save`.

- [x] **21. Make replay compatibility and divergence checking explicit.**
      The replay header has a format version but no simulation-build/content
      fingerprint. Release floating-point options also permit differences between
      builds. The digest hashes a limited summary: transforms, ownership/kind/health,
      clock, RNG draw count, and resources; orders, cooldowns, production queues,
      mission state, and much other authoritative state are omitted. Even collected
      `max_health` is not mixed into `world_digest`. Record compatibility identifiers
      and reject incompatible playback; derive a canonical digest from the
      authoritative-state contract. Test intentional changes to previously omitted
      fields. Keep same-binary replay as the current supported determinism contract.
      Evidence: [replay.h](game/command/replay.h), `ReplayHeader`;
      [world_digest.cpp](game/session/world_digest.cpp);
      [CMakeLists.txt](CMakeLists.txt), floating-point flags.

- [x] **22. Split component definitions and consolidate state-projection schemas.**
      `component.h` is 2,790 lines and is directly included by hundreds of source
      files. It combines core, combat, economy, commander, and presentation types;
      changing one domain can trigger broad rebuilds. Serialization, snapshot copy
      lists, and presentation signatures separately enumerate state. Split headers
      by domain and use explicit schema/traits for applicable projections, retaining
      the existing snapshot classification checks. Verify narrower incremental
      rebuilds and tests that catch an authoritative/rendered field added without
      corresponding serialization/copy/invalidation behavior.
      Evidence: [component.h](game/core/component.h);
      [world.cpp](game/core/world.cpp), snapshot copy/signature helpers;
      [serialization.cpp](game/save/serialization.cpp);
      [snapshot_contract.h](game/save/snapshot_contract.h).

- [ ] **23. Reduce application coordination and QML duplication by responsibility.**
      `GameEngine` is 3,272 lines and combines threading, lifecycle, persistence,
      mission progression, and UI synchronization. `CommanderControlController` is
      2,892 lines with input, locomotion, targeting, and combat concerns.
      `ProductionPanel.qml` is 3,106 lines with repeated building-card structures and
      roster/fallback data. Existing view models help, but the shared `ClientContext`
      still exposes a broad set of mutable subsystem pointers. Extract lifecycle
      coordination, simulation commander logic, and reusable data-backed QML cards;
      give collaborators narrow dependencies. Preserve interaction tests and measure
      QML creation/binding costs before claiming a runtime improvement.
      Progress (2026-09-06): the save orchestration came out of `GameEngine` into
      `App::Core::SaveOrchestrator`, which owns the queued request, its mutex and the
      capture timing and depends on nothing but three callbacks. `GameEngine` keeps
      the thin Q_INVOKABLE surface. `CommanderControlController` and
      `ProductionPanel.qml` are untouched, and `ClientContext` still exposes the same
      broad set of pointers. Barely started.
      Evidence: [game_engine.cpp](app/core/game_engine.cpp);
      [commander_control_controller.cpp](app/commander/commander_control_controller.cpp);
      [ProductionPanel.qml](ui/qml/ProductionPanel.qml);
      [client_context.h](app/core/client_context.h).

- [x] **24. Share real match initialization with the headless host.**
      `soi_headless` accepts a predefined battlefield scenario and only its own replay
      kind. Real skirmish setup remains in `app/session/skirmish_loader.cpp`, alongside
      client setup. This limits how faithfully headless tests can exercise shipped
      map/player/stock/wall initialization and prevents the existing host from simply
      loading a real client match. Extract a renderer-independent match launch/setup
      service and let both hosts use it. Verify the same map, player configuration,
      and seed create matching authoritative initial state.
      Evidence: [main.cpp](tools/headless/main.cpp);
      [skirmish_loader.cpp](app/session/skirmish_loader.cpp).

- [x] **25. Measure the complete tick and make GPU timing collection nonblocking.**
      `World::update` starts its tick timer after initial motion presentation work and
      ends it before final presentation, movement tracing, and render-snapshot
      publication. Those costs can be significant but are absent from its reported
      total. GPU profiling uses two frame slots and waits on a fence before collecting
      results, so enabling timing can itself impose a wait. Add end-to-end tick and
      publication timers, distinguish lock waits, and retrieve available GPU queries
      asynchronously with sample-age metadata. Compare profiling on/off overhead and
      reconcile reported totals with wall time.
      Evidence: [world.cpp](game/core/world.cpp), `update`;
      [backend.cpp](render/gl/backend.cpp), `wait_for_frame_slot`, `execute_scene`;
      [backend.h](render/gl/backend.h), `k_frames_in_flight`.

- [x] **26. Extend regression budgets beyond simulation amplification counts.**
      PR CI runs `check-sim-budgets.py`, but deliberately does not gate timing or
      cross-build digests. The checked-in fixtures cover 1,000/2,000 simulation units;
      they do not establish hardware rendering, fallback, allocation, save-stall,
      or multi-mission residency limits. Keep deterministic counter gates on shared
      runners and add reproducible scheduled/dedicated-hardware runs for frame/tick
      percentiles, memory, publication cost, and gameplay latency. Record hardware,
      quality, warmup, commit, and content identity with each baseline.
      Evidence: [pr.yml](.github/workflows/pr.yml), simulation-budget lane;
      [sim_budgets.json](tests/perf/sim_budgets.json);
      [PERFORMANCE_INSTRUMENTATION.md](docs/PERFORMANCE_INSTRUMENTATION.md).

- [x] **27. Update architecture claims and broaden guards that miss current patterns.**
      `ARCHITECTURE.md` still states 65 ambient sites and zero in the app; the current
      checker reports 58 and the budget contains two in `app/world`. It says the pair
      contact cache was removed, but `g_contact_cache` now exists. It also describes
      `render_gl` consuming `game_systems`, whereas the root CMake now links it to
      `game_sim`. Generate counts/dependency summaries or verify them in documentation
      checks. Expand the world-scan guard beyond the spelling
      `collect_entities_with` to account for expensive owner queries/nested loops,
      preferably using runtime candidate counters. Passing current guards does not
      establish that all costly scans or ambient registries are covered.
      Evidence: [ARCHITECTURE.md](docs/ARCHITECTURE.md);
      [CMakeLists.txt](CMakeLists.txt), `target_link_libraries(render_gl PUBLIC game_sim)`;
      [check-world-scans.py](scripts/check-world-scans.py);
      [check-ambient-instances.py](scripts/check-ambient-instances.py).

## Suggested sequence and audit checks

Address 01–03 first. Establish tick-owned mission/commander input and coherent
snapshot boundaries before attempting wider simulation concurrency. Fix
session/cache identity before adding more caches. Then use current end-to-end
profiles to choose among contact geometry, AI queries, GPU work, and residency.
Simulation units and rendered soldiers are different workload measures; do not
compare their benchmark counts as if they were interchangeable.

Read-only checks run during this audit:

- `python3 scripts/check-modules.py` — passed.
- `python3 scripts/check-render-boundary.py` — passed.
- `python3 scripts/check-command-boundary.py` — passed.
- `python3 scripts/check-ambient-instances.py` — passed, 58 tracked call sites.
- `python3 scripts/check-world-scans.py` — passed, 68 tracked scans.

Existing sparse component storage, spatial indexing, fixed-step simulation,
command validation, detached render worlds, shared rigged assets, BPAT playback,
parallel humanoid preparation, save workers, and architecture tests are already
implemented. The tasks above target their remaining limits; they are not requests
to rebuild those systems from scratch. No runtime code was changed, and no fresh
benchmark or full gameplay test run was performed for this documentation audit.
