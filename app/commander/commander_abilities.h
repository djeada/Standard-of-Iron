#pragma once

#include <cstdint>

namespace Engine::Core {
class World;
class Entity;
class CommanderComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

class CommanderMotor;

struct CommanderAbilityRequest {
  bool shield_bash{false};
  bool vanguard_rush{false};
  bool second_wind{false};

  [[nodiscard]] auto any() const -> bool {
    return shield_bash || vanguard_rush || second_wind;
  }
};

struct CommanderAbilityContext {
  Engine::Core::World* world{nullptr};
  Engine::Core::Entity* commander{nullptr};
  Engine::Core::EntityID commander_id{0};
  int local_owner_id{0};
  float view_yaw{0.0F};
  bool dodging{false};
  bool airborne{false};
  Engine::Core::EntityID locked_target_id{0};
  Engine::Core::EntityID soft_target_id{0};
  CommanderMotor* motor{nullptr};
};

struct CommanderAbilityOutcome {
  bool rescan_primary_target{false};
};

class CommanderAbilities {
public:
  void advance_cooldowns(Engine::Core::CommanderComponent* commander, float dt);

  auto activate(const CommanderAbilityRequest& request,
                const CommanderAbilityContext& context) -> CommanderAbilityOutcome;

  [[nodiscard]] auto shield_bash_cooldown() const -> float {
    return m_shield_bash_cooldown;
  }
  [[nodiscard]] auto vanguard_rush_cooldown() const -> float {
    return m_vanguard_rush_cooldown;
  }
  [[nodiscard]] auto second_wind_cooldown() const -> float {
    return m_second_wind_cooldown;
  }

  void reset();

private:
  [[nodiscard]] auto resolve_target(const CommanderAbilityContext& context,
                                    float max_range) const -> Engine::Core::EntityID;

  auto try_shield_bash(const CommanderAbilityContext& context) -> bool;
  auto try_vanguard_rush(const CommanderAbilityContext& context) -> bool;
  auto try_second_wind(const CommanderAbilityContext& context) -> bool;

  float m_shield_bash_cooldown{0.0F};
  float m_vanguard_rush_cooldown{0.0F};
  float m_second_wind_cooldown{0.0F};
};

} // namespace App::Core
