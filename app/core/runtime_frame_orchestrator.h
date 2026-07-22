#pragma once

#include <functional>

#include "app_scene_context.h"
#include "entity_cache.h"

class AmbientStateManager;
class QString;

struct FrameUpdateCallbacks {
  std::function<void()> on_minimap_image_changed;
  std::function<void()> on_selected_units_data_changed;
};

struct RuntimeFrameState {
  int local_owner_id = 1;
  bool spectator_mode = false;
  int viewport_width = 0;
  int viewport_height = 0;
  bool selection_refresh_enabled = false;
  int selection_refresh_counter = 0;
  float minimap_unit_update_accumulator = 0.0F;
  float simulation_accumulator = 0.0F;
};

class RuntimeFrameOrchestrator {
public:
  using SimulationStep = std::function<void(float)>;

  void update(const AppSceneContext& scene,
              RuntimeFrameState& state,
              EntityCache& entity_cache,
              AmbientStateManager* ambient_state_manager,
              const QString& victory_state,
              float dt,
              const FrameUpdateCallbacks& callbacks,
              const SimulationStep& simulation_step) const;
};
