#pragma once

#include <cstdint>
#include <vector>

#include "../core/entity.h"
#include "../units/spawn_type.h"
#include "nature_ai.h"
#include "wildlife_group.h"
#include "wildlife_species.h"
#include "wildlife_threats.h"

namespace Engine::Core {
class World;
}

namespace Game::Wildlife {

[[nodiscard]] auto is_civilian_spawn(Game::Units::SpawnType type) -> bool;

enum class Tier : std::uint8_t {
  Near = 0,
  Far = 1,
  Dormant = 2,
};

struct GroupRuntime {
  float center_x{0.0F};
  float center_z{0.0F};
  float alarm{0.0F};
  int alive{0};
};

struct AnimalRef {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  std::uint16_t group{0U};
  Species species{Species::Sheep};
};

struct QuarryRef {
  Engine::Core::EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  bool civilian{false};
};

class WildlifeCensus {
public:
  void reset();

  void rebuild_threats(Engine::Core::World& world);
  void collect_animals(Engine::Core::World& world,
                       const std::vector<GroupState>& groups);

  [[nodiscard]] auto tier_for(float world_x,
                              float world_z,
                              float near_radius,
                              float far_radius) const -> Tier;

  [[nodiscard]] auto animals() const noexcept -> const std::vector<AnimalRef>& {
    return m_animals;
  }
  [[nodiscard]] auto threats() const noexcept -> const ThreatField& {
    return m_threats;
  }
  [[nodiscard]] auto runtime_of(std::size_t group_index) const -> const GroupRuntime*;
  [[nodiscard]] auto runtime_for_group(const std::vector<GroupState>& groups,
                                       std::uint16_t group_id) const -> GroupRuntime;
  [[nodiscard]] auto alive_in(std::size_t group_index) const -> int;

  void alert_group(std::uint16_t group_id, float duration);
  void
  rally_pack(std::uint16_t group_id, Engine::Core::EntityID foe_id, float duration);

  [[nodiscard]] auto nearest_prey(float world_x,
                                  float world_z,
                                  float radius,
                                  Engine::Core::EntityID hunter_id,
                                  float appetite) const -> const AnimalRef*;
  [[nodiscard]] auto nearest_quarry(float world_x,
                                    float world_z,
                                    float radius,
                                    Engine::Core::EntityID hunter_id,
                                    float appetite) const -> const QuarryRef*;
  [[nodiscard]] auto pack_slot_for(Engine::Core::EntityID prey_id,
                                   Engine::Core::EntityID hunter_id) const -> PackSlot;
  [[nodiscard]] auto
  nearest_pack_hunter(float world_x, float world_z, float radius) const -> ThreatQuery;

private:
  [[nodiscard]] auto attackers_on(Engine::Core::EntityID prey_id,
                                  Engine::Core::EntityID exclude_id) const -> int;

  std::vector<GroupRuntime> m_group_runtime;
  std::vector<AnimalRef> m_animals;
  std::vector<QuarryRef> m_quarry;
  ThreatField m_threats;
  ThreatField m_interest;
};

} // namespace Game::Wildlife
