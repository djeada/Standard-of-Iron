#pragma once

#include "../core/system.h"
#include "../core/world.h"

namespace Game::Systems {

/// Diagnostics for target commitment.
struct TargetCommitmentDiagnostics {
  std::uint32_t switches_blocked{0};
  std::uint32_t switches_allowed{0};
  std::uint32_t forced_releases{0};
};

/// Target commitment system (Workstream 5).
///
/// Prevents units from switching targets during committed attack phases
/// (wind-up, strike, impact). Allows switching only when the current target
/// is dead, invalid, unreachable, or after recovery.
class TargetCommitmentSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto diagnostics() const -> const TargetCommitmentDiagnostics& {
    return m_diagnostics;
  }

private:
  TargetCommitmentDiagnostics m_diagnostics;
};

} // namespace Game::Systems
