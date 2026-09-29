# app/ navigation guide

`app/` is the Qt/QML application shell. It translates user intent into commands, coordinates presentation, owns the match lifecycle on the client, and hosts the frame loop. It does not own gameplay rules: authoritative state lives in `game/` (see [game/README.md](../game/README.md)), and gameplay mutations go through `game/command/`. The dependency picture is in [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md).

## Where things live

| Directory      | Owns                                                                                                                                                                                                                                                               | Consumes                                                              | Start reading at                                                           |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| `core/`        | `GameEngine` (Qt-facing facade, frame entry points, composition), `SimulationLifecycle` (sim thread, frame mutex, `FrameBarrier`), `Published<T>`, `PresentationFrame`, frame/presentation orchestrators, `OrderFeedbackPresenter`, `AudioServices`, film recorder | everything below                                                      | `game_engine.h`, `game_engine_composition.cpp`, `game_engine_wiring.cpp`   |
| `input/`       | Pointer/keyboard dispatch, cursor, hover, RTS camera                                                                                                                                                                                                               | `orders/`, `world/`                                                   | `input_command_handler.h`                                                  |
| `orders/`      | Gesture to intent to command submission, accepted/rejected feedback, order markers, army-formation placement                                                                                                                                                       | `game/command/`, `game/systems/` services                             | `command_controller.h`, `order_submission.h`                               |
| `economy/`     | Building/wall placement gestures and previews, production readouts, harvest targeting                                                                                                                                                                              | `game/systems/*placement*`, `game/systems/economy/production_service` | `production_manager.h`                                                     |
| `commander/`   | Direct-control commander mode: input, motor, camera rig, abilities, status                                                                                                                                                                                         | `game/command/`, camera                                               | `commander_control_controller.h`                                           |
| `session/`     | Match loading: level/skirmish loaders, renderer and world bootstrap, loading progress and overlay, environment and weather runtime, replay arming                                                                                                                  | `game/map/`, `game/session/`                                          | `level_orchestrator.h`, `skirmish_loader.h`, `loading_overlay.h`           |
| `persistence/` | Save queueing and capture, save-slot progress/autosave/screenshot state, load orchestration, state restore                                                                                                                                                         | `game/save/`, `game/systems/persistence/save_load_service`            | `save_slot_controller.h`, `save_orchestrator.h`, `save_load_coordinator.h` |
| `viewmodels/`  | QML-facing view models (`Q_PROPERTY`/`Q_INVOKABLE`)                                                                                                                                                                                                                | `core/client_context.h`                                               | `orders_view_model.h`, `match_setup_view_model.h`                          |
| `world/`       | Client-side world-derived UI state: minimap manager and events, battle stats and defeat watcher, targeting highlights, focus targets, ally announcements, ambient state, visibility                                                                                | `game/render_bridge/`                                                 | `minimap_manager.h`, `targeting_presentation.h`                            |
| `models/`      | QML list models and image providers                                                                                                                                                                                                                                |                                                                       | `selected_units_model.h`                                                   |
| `audio/`       | Audio coordinator, resource loading, weather audio                                                                                                                                                                                                                 | `game/audio/`                                                         | `audio_coordinator.h`                                                      |
| `mission/`     | Mission runtime (stages, waves, announcements), commander messages, tutorial runtime and observation                                                                                                                                                               | `game/mission/`                                                       | `mission_runtime.h`, `tutorial_observation.h`                              |
| `utils/`       | Screen-to-ground helpers                                                                                                                                                                                                                                           |                                                                       | `engine_view_helpers.h`                                                    |

## Threads

- **GUI/QML thread**: input, view models, loading orchestration, save completion. Input handlers take `GameEngine::lock_frame()` (a recursive mutex).
- **Simulation thread** (`QThread` named "SoISimulation"): `simulate`, `update_presentation`, command drain, save capture, presentation publication.
- **Render thread** (Qt Quick scene graph): `GameEngine::render`. It reads published immutable data only.

`FrameBarrier` arbitrates freeze/simulation/render phases; `WorldFreeze` is the RAII handle for destructive world replacement.

## Worked call paths

Each step is written as a file and a symbol. `scripts/check-navigation-guides.py` (part of `scripts/check-pr-policy.py`) fails when a file or symbol named here stops existing, so keep the format `` `path` `Symbol` ``.

### 1. Issuing an order (right-click move or attack)

1. `ui/qml/GameView.qml` `on_right_release`: QML forwards the gesture to `game.orders`.
2. `app/viewmodels/orders_view_model.cpp` `OrdersViewModel::on_right_release`: ensures the engine is initialized, takes the frame lock, calls the input handler.
3. `app/input/input_command_handler.cpp` `InputCommandHandler::on_right_click`: spectator early-out, cancels order modes, picks an enemy, resolves context interaction (gather, food, repair) for builders and civilians, otherwise routes to the command controller.
4. `app/orders/command_controller.cpp` `CommandController::on_move_or_attack_click`: delegates target resolution and command building.
5. `app/orders/movement_utils.cpp` `issue_move_or_attack_command`: enemy hit builds `AttackTarget`, ground hit plans a `Move` (drops loaded units, checks walkability via `CommandService::plan_ground_move`).
6. `app/orders/order_submission.cpp` `submit_player_order`: wraps the payload in a `Command{LocalPlayer, owner, payload}`, validates with `Game::Command::validate`, and submits. A refusal returns a `Rejected` `OrderOutcome`.
7. `game/command/command_queue.cpp` `submit`: resolves the world's `SessionContext` and pushes onto the pending queue under a mutex.
8. `game/command/command_system.cpp` `CommandSystem::update` (simulation thread): drains the queue at the start of the tick; `CommandQueue::drain` re-validates each command against current world state and reports late rejections.
9. `game/command/command_dispatcher.cpp` `dispatch`: `std::visit` over the payload (`apply_move` in `command_unit_orders.cpp` releases formation membership then calls `CommandService::move_units`); movement and combat systems then act on the changed components.
10. Feedback: `app/orders/order_issuer.cpp` `OrderIssuer::publish` fires the sink; `app/core/order_feedback_presenter.cpp` `OrderFeedbackPresenter::present` (called by `GameEngine::handle_order_feedback`) pushes an order marker, plays the accept/refuse cue, spawns attack VFX and returns what the engine emits as the QML `order_feedback` signal. Late rejections arrive through `GameEngine::report_late_command_rejection` (simulation thread, re-queued to the GUI thread).

### 2. Advancing a frame

1. `ui/gl_view.cpp` `GLView::GLRenderer::render`: on the render thread, begins the render frame through the barrier and lazily starts the simulation thread.
2. `app/core/simulation_lifecycle.cpp` `SimulationLifecycle::start` (called by `GameEngine::start_simulation_thread`): creates the simulation `QThread`.
3. `app/core/simulation_lifecycle.cpp` `SimulationLifecycle::run`: per 1/60 s tick, clamp `dt` to 0.1 s, take the frame mutex, `FrameBarrier::try_begin_simulation` (skip the tick during a freeze), then run the tick body `GameEngine::run_simulation_tick` (`simulate`, `update_presentation`, `drain_pending_capture`), end of tick.
4. `app/core/runtime_frame_orchestrator.cpp` `RuntimeFrameOrchestrator::advance_simulation`: `SessionContext::advance` runs fixed ticks (mission waves, mission stages, `World::update`, victory), then publishes the render snapshot.
5. `app/core/game_engine.cpp` `GameEngine::update_presentation` (still the simulation thread): order markers, defeat announcements and `AllyAnnouncementPresenter::announce_all`, `RuntimeFrameOrchestrator::update` (ambient, animation time, camera, visibility, minimap, selection), view-model publication through `Published<T>`, targeting and range-ring sync via `MatchPresentationSync`, then `publish_presentation_frame` (atomic release store of the `PresentationFrame`).
6. `app/core/game_engine.cpp` `GameEngine::render` (render thread): atomic acquire of the `PresentationFrame`, sets the world view, submits terrain and the world, acquires the world render snapshot, then `FrameUiCoordinator::render_effects`, end frame, loading overlay and cursor updates.

Teardown order: `GLView::GLRenderer::~GLRenderer` calls `GameEngine::stop_simulation_thread` (`SimulationLifecycle::stop` clears the running flag, joins) before releasing OpenGL resources; `GameEngine::~GameEngine` then calls `SaveSlotController::shutdown` (stops the autosave timer, waits for pending saves, shuts down the save worker), then `AudioServices::shutdown`.

### 3. Loading a match

1. `ui/qml/Main.qml` `start_skirmish`: QML calls `game.setup`.
2. `app/viewmodels/match_setup_view_model.cpp` `MatchSetupViewModel::start_skirmish`: builds a `MatchLaunch` and emits `launch_requested`, connected in composition to the engine.
3. `app/core/game_engine.cpp` `GameEngine::launch_match`: sets speed and replay launch, then starts the skirmish.
4. `app/core/game_engine.cpp` `GameEngine::start_skirmish_internal`: acquires a `WorldFreeze`, resets mission state, marks loading (`begin_match_loading`), and defers the heavy work (`complete_match_load`) with `QTimer::singleShot`. The lambda keeps the freeze alive until it finishes.
5. `app/session/skirmish_runtime_coordinator.cpp` `SkirmishRuntimeCoordinator::perform_load`: creates the `LevelOrchestrator`.
6. `app/session/level_orchestrator.cpp` `LevelOrchestrator::load_skirmish`: steps `LoadingProgressTracker` through map data, terrain, biome, water, roads, environment, fog, entities, audio, minimap, systems, finalize.
7. `app/session/skirmish_loader.cpp` `SkirmishLoader::start`: resets state, acquires the parsed map via `MapContextStore` (`MapLoader::load_from_json_file`), applies owner and nation overrides, loads the level and initializes the nav grid.
8. `app/session/level_loader.cpp` `LevelLoader::loadFromAssets`: wraps `Game::Map::load_match` (`game/map/match_loader.cpp`), which populates the world through `MapTransformer::apply_to_world` and applies the environment.
9. `app/session/skirmish_runtime_coordinator.cpp` `SkirmishRuntimeCoordinator::finalize_load`: clears the loading flag, arms the first-frame overlay countdown, rebuilds the entity cache, arms replay recording/verification.
10. First frame: the simulation thread skips `simulate`/`update_presentation` while `m_runtime.loading` is set. `GameEngine::update_loading_overlay` releases the overlay after the frame count and minimum time pass and startup readiness holds (bounded by a maximum wait).

The `SessionContext` itself is created earlier, in the `GameEngine` constructor by `app/core/game_engine_composition.cpp` `GameEngine::build_client_and_view_models`.

### 4. Saving and restoring

Save:

1. QML signals `save_requested`, `quicksave_requested` or `autosave_requested` on the save-slots view model, or the autosave timer fires; `SaveSlotController` handles them (GUI thread).
2. `app/persistence/save_slot_controller.cpp` `SaveSlotController::begin_save`: rejects concurrent saves, leaves commander mode, queues the capture.
3. `app/persistence/save_orchestrator.cpp` `SaveOrchestrator::queue`: holds one pending capture while the simulation runs.
4. `app/persistence/save_orchestrator.cpp` `SaveOrchestrator::drain` (simulation thread, end of tick): runs the capture callback, then bounces delivery to the GUI thread.
5. `app/persistence/save_load_coordinator.cpp` `SaveLoadCoordinator::begin_save_to_slot`: builds metadata and the session snapshot, calls `Serialization::serialize_world`, verifies the tick/RNG capture stamp (a mismatch is a torn capture and the save is refused), hands a `SaveRequest` to the service.
6. `game/systems/persistence/save_load_service.cpp` `SaveLoadService::begin_save`: enqueues a job for the worker thread.
7. `game/systems/persistence/save_load_service.cpp` `SaveLoadService::worker_loop`: the worker owns its `SaveStorage`, packs the JSON (`Save::pack`), writes the slot, prunes autosaves, emits progress and completion.
8. `app/persistence/save_slot_controller.cpp` `SaveSlotController::finish_request`: back on the GUI thread, requests the screenshot, which the render thread attaches to the slot.

Restore:

1. `app/core/game_engine.cpp` `GameEngine::load_game_from_slot`: takes a `WorldFreeze`, marks loading.
2. `app/persistence/save_load_coordinator.cpp` `SaveLoadCoordinator::load_from_slot`: waits for AI decisions, then the service reads and unpacks the slot, deserializes into a staging world first, and only then clears and deserializes into the live world.
3. Same function: restores player nations, level snapshot, runtime snapshot (economy, clock tick, RNG), environment and camera, rebuilds registries, re-acquires the map context, reinitializes AI, restores the map session contributors and seals terrain.
4. `app/core/game_engine.cpp` `GameEngine::end_match_after_failed_load`: used when the world was already discarded and the load failed.

## Conventions

- Prefer collaborators that take the narrow references they need over the whole `ClientContext`; it is the view-model seam, not a service locator.
- Qt/QML conversion (`QVariantMap`, `QString` for display) belongs at view-model boundaries; controllers should produce typed results.
- New QML files, singletons, icons, properties and translated strings each have registration steps (see the notes in `docs/UI_DESIGN_SYSTEM.md`, `CMakeLists.txt` `QML_FILES`, and `tests/architecture/qml_surface_test.cpp`).
