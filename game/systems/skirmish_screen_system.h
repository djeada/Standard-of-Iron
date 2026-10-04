#pragma once

#include <unordered_map>

#include "../core/entity.h"
#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

class OwnerRegistry;

class SkirmishScreenSystem : public Engine::Core::System {
public:
  static constexpr float k_threat_radius = 7.5F;
  static constexpr float k_line_search_radius = 30.0F;
  static constexpr float k_behind_line_distance = 4.5F;
  static constexpr float k_open_ground_retreat = 7.0F;
  static constexpr float k_withdraw_cooldown = 3.0F;

  struct Services {
    OwnerRegistry& owners;
  };

  explicit SkirmishScreenSystem(Services services)
      : m_owners(services.owners) {}
  ~SkirmishScreenSystem() override = default;

  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

private:
  OwnerRegistry& m_owners;
  float m_clock{0.0F};
  std::unordered_map<Engine::Core::EntityID, float> m_last_withdraw;
};

} // namespace Game::Systems
