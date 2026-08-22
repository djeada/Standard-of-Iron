#pragma once

#include <cstdint>

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

struct TargetCommitmentDiagnostics {
  std::uint32_t switches_blocked{0};
  std::uint32_t switches_allowed{0};
  std::uint32_t forced_releases{0};
};

class TargetCommitmentSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] auto diagnostics() const -> const TargetCommitmentDiagnostics& {
    return m_diagnostics;
  }

private:
  TargetCommitmentDiagnostics m_diagnostics;
};

} // namespace Game::Systems
