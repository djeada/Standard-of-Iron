#pragma once

#include <QJsonObject>

#include <unordered_map>
#include <vector>

#include "../core/system.h"
#include "army_formation_types.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Formation {

struct ArmyFormationPlan;

class ArmyFormationRegistry {
public:
  static auto instance() -> ArmyFormationRegistry&;

  auto create_group(FormationDoctrineId doctrine,
                    ArmyFormationIntent intent,
                    std::vector<EntityID> members) -> FormationGroupID;

  [[nodiscard]] auto find(FormationGroupID id) -> ArmyFormation*;
  [[nodiscard]] auto find(FormationGroupID id) const -> const ArmyFormation*;

  void remove_group(FormationGroupID id);

  auto add_member(FormationGroupID id, EntityID entity) -> bool;
  auto remove_member(EntityID entity) -> bool;

  [[nodiscard]] auto group_of(EntityID entity) const -> FormationGroupID;

  void apply_plan(FormationGroupID id, const ArmyFormationPlan& plan);

  [[nodiscard]] auto group_ids() const -> std::vector<FormationGroupID>;
  [[nodiscard]] auto group_count() const -> std::size_t { return m_groups.size(); }

  void clear();

  [[nodiscard]] auto to_json() const -> QJsonObject;
  void from_json(const QJsonObject& root);

private:
  void reindex_membership(const ArmyFormation& formation);

  std::unordered_map<FormationGroupID, ArmyFormation> m_groups;
  std::unordered_map<EntityID, FormationGroupID> m_membership;
  FormationGroupID m_next_id{1U};
};

class ArmyFormationRuntime : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  static void sync_membership_components(Engine::Core::World& world,
                                         const ArmyFormation& formation);

  static void detach(Engine::Core::World& world, EntityID entity);

  [[nodiscard]] static auto replan(Engine::Core::World& world,
                                   FormationGroupID id) -> bool;

  [[nodiscard]] static auto
  move_speed_multiplier(const Engine::Core::Entity& entity) -> float;

  static void begin_move(Engine::Core::World& world,
                         FormationGroupID id,
                         const QVector3D& destination,
                         float facing);

private:
  void advance_maintained_groups(Engine::Core::World& world, float delta_time);

  float m_replan_accumulator{0.0F};
  float m_advance_accumulator{0.0F};
};

} // namespace Game::Formation
