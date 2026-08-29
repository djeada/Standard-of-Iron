#pragma once

#include <cstdint>

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

struct BodyContactDiagnostics {
  std::uint32_t pairs_resolved{0};
  std::uint32_t pushes_rejected{0};
  float deepest_overlap{0.0F};
};

class BodyContactSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] auto diagnostics() const -> const BodyContactDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_stale_position_margin = 0.5F;
  static constexpr float k_stale_position_speed_allowance = 12.0F;

private:
  BodyContactDiagnostics m_diagnostics;
};

} // namespace Game::Systems
