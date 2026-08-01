#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/system.h"
#include "../core/world.h"

namespace Game::Systems {

struct LocalAvoidanceDiagnostics {
  float spatial_hash_build_ms{0.0F};
  std::uint32_t average_neighbors_checked{0};
  std::uint32_t units_processed{0};
  std::uint32_t overlaps_detected{0};
};

class LocalAvoidanceSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto diagnostics() const -> const LocalAvoidanceDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_default_cell_size = 4.0F;
  static constexpr float k_separation_radius = 2.0F;
  static constexpr float k_max_correction_per_tick = 0.3F;
  static constexpr float k_separation_strength = 1.5F;

private:
  struct UnitCircle {
    Engine::Core::EntityID id{0};
    float x{0.0F};
    float z{0.0F};
    float radius{0.5F};
    float vx{0.0F};
    float vz{0.0F};
    std::uint8_t priority{0};
    bool is_moving{false};
  };

  static auto cell_key(int cell_x, int cell_z) -> std::int64_t;

  LocalAvoidanceDiagnostics m_diagnostics;
  std::unordered_map<std::int64_t, std::vector<std::size_t>> m_grid;
  std::vector<std::int64_t> m_active_cell_keys;
  std::vector<UnitCircle> m_circles;
  std::vector<Engine::Core::Entity*> m_query_scratch;
  std::size_t m_previous_cell_count{0};
};

} // namespace Game::Systems
