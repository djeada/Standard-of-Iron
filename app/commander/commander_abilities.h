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

struct CommanderRefusal {
  std::uint32_t sequence{0};
  const char* action{""};
};

// Cooldowns live on CommanderComponent and run down in CommanderSystem in
// either control mode; this class only decides and starts abilities.
class CommanderAbilities {
public:
  auto activate(const CommanderAbilityRequest& request,
                const CommanderAbilityContext& context) -> CommanderAbilityOutcome;

  void reset() {}

  void refuse(const char* action);
  [[nodiscard]] auto last_refusal() const -> const CommanderRefusal& {
    return m_last_refusal;
  }

private:
  [[nodiscard]] auto resolve_target(const CommanderAbilityContext& context,
                                    float max_range) const -> Engine::Core::EntityID;

  auto try_shield_bash(const CommanderAbilityContext& context) -> bool;
  auto try_vanguard_rush(const CommanderAbilityContext& context) -> bool;
  auto try_second_wind(const CommanderAbilityContext& context) -> bool;

  CommanderRefusal m_last_refusal;
};

} // namespace App::Core
