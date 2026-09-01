#pragma once

#include <QVector3D>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include "../../core/entity.h"
#include "../../units/combat_role.h"

namespace Engine::Core {
class World;
class AttackComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

struct CandidateRecord {
  std::uint32_t stamp{0};
  Engine::Core::EntityID id{0};
  Engine::Core::Entity* entity{nullptr};
  int owner_id{0};
  bool is_building{false};
  bool is_wildlife{false};
};

struct CombatQueryContext {
  CombatQueryContext();

  void clear();

  void record_candidate(Engine::Core::Entity* entity, int owner_id, bool building);

  [[nodiscard]] auto
  find_record(Engine::Core::EntityID entity_id) const -> const CandidateRecord*;

  [[nodiscard]] auto
  find_entity(Engine::Core::EntityID entity_id) const -> Engine::Core::Entity*;

  [[nodiscard]] auto hostile(int attacker_owner_id, int target_owner_id) const -> bool;

  std::vector<Engine::Core::Entity*> units;

  Engine::Core::World* world{nullptr};
  mutable std::vector<Engine::Core::EntityID> nearby_unit_ids;

private:
  static constexpr int k_max_cached_owner_id = 64;
  static constexpr std::size_t k_owner_axis =
      static_cast<std::size_t>(k_max_cached_owner_id) + 1U;
  static constexpr std::uint8_t k_hostility_unknown = 0U;
  static constexpr std::uint8_t k_hostility_hostile = 1U;
  static constexpr std::uint8_t k_hostility_friendly = 2U;

  friend void rebuild_combat_query_context(Engine::Core::World* world,
                                           CombatQueryContext& query_context);

  void rebuild_hostility_table();

  std::vector<CandidateRecord> m_records;
  std::uint32_t m_stamp{0};
  std::vector<std::uint8_t> m_hostility;
  std::vector<int> m_present_owner_ids;
};

void collect_unit_ids_near(Engine::Core::World& world,
                           float x,
                           float z,
                           float radius,
                           std::vector<Engine::Core::EntityID>& out);

auto build_combat_query_context(Engine::Core::World* world) -> CombatQueryContext;

void rebuild_combat_query_context(Engine::Core::World* world,
                                  CombatQueryContext& query_context);

auto is_unit_in_hold_mode(Engine::Core::Entity* entity) -> bool;

auto is_unit_in_guard_mode(Engine::Core::Entity* entity) -> bool;

auto is_building(Engine::Core::Entity* entity) -> bool;

auto is_valid_enemy_unit(const Engine::Core::UnitComponent* attacker_unit,
                         Engine::Core::Entity* target,
                         bool allow_buildings) -> bool;

auto is_valid_enemy_of_owner(int attacker_owner_id,
                             Engine::Core::Entity* target,
                             bool allow_buildings) -> bool;

auto is_passive_wildlife(Engine::Core::Entity* target) -> bool;

auto is_auto_acquirable_enemy(const Engine::Core::UnitComponent* attacker_unit,
                              Engine::Core::Entity* target,
                              bool allow_buildings) -> bool;

auto is_auto_acquirable_enemy_of_owner(int attacker_owner_id,
                                       Engine::Core::Entity* target,
                                       bool allow_buildings) -> bool;

auto combat_radius(Engine::Core::Entity* entity) -> float;

auto is_in_range(Engine::Core::Entity* attacker,
                 Engine::Core::Entity* target,
                 float range) -> bool;

auto structure_separates_positions(const QVector3D& from, const QVector3D& to) -> bool;

auto structure_separates_combatants(Engine::Core::Entity* attacker,
                                    Engine::Core::Entity* target) -> bool;

auto melee_bypass_destination(const QVector3D& attacker_position,
                              const QVector3D& target_position,
                              float standoff_distance,
                              float clearance_radius) -> std::optional<QVector3D>;

auto melee_walled_off_from(Engine::Core::Entity* attacker,
                           Engine::Core::Entity* target) -> bool;

auto suppresses_opportunistic_combat(Engine::Core::Entity* unit) -> bool;

auto combat_role_of(const Engine::Core::Entity* entity) -> Game::Units::CombatRole;

auto auto_acquires_targets(Engine::Core::Entity* entity) -> bool;

auto answers_threat_alerts(Engine::Core::Entity* entity) -> bool;

auto pursues_targets(const Engine::Core::Entity* entity) -> bool;

auto opens_fire_without_closing(const Engine::Core::Entity* entity) -> bool;

auto acquisition_range(Engine::Core::Entity* entity) -> float;

auto is_unit_idle(Engine::Core::Entity* unit) -> bool;

auto should_auto_engage_melee(Engine::Core::Entity* unit) -> bool;

enum class EngagementTrigger : std::uint8_t {
  Opportunity,
  Retaliation,
  SquadAlert,
  SightAlert,
};

auto may_engage(Engine::Core::Entity* unit,
                Engine::Core::Entity* enemy,
                EngagementTrigger trigger) -> bool;

class TargetFilter {
public:
  TargetFilter() = default;

  template <typename Fn>
    requires(!std::same_as<std::decay_t<Fn>, TargetFilter>)
  TargetFilter(const Fn& predicate)
      : m_context(&predicate)
      , m_invoke([](const void* context, Engine::Core::Entity* candidate) {
        return (*static_cast<const Fn*>(context))(candidate);
      }) {}

  [[nodiscard]] explicit operator bool() const { return m_invoke != nullptr; }

  auto operator()(Engine::Core::Entity* candidate) const -> bool {
    return m_invoke(m_context, candidate);
  }

private:
  const void* m_context{nullptr};
  bool (*m_invoke)(const void*, Engine::Core::Entity*){nullptr};
};

auto find_nearest_enemy(Engine::Core::Entity* unit,
                        const CombatQueryContext& query_context,
                        float max_range,
                        std::uint64_t* scan_iterations = nullptr,
                        const TargetFilter& accept = {}) -> Engine::Core::Entity*;

} // namespace Game::Systems::Combat
