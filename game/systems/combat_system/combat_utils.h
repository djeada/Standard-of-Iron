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
#include "../../units/spawn_type.h"
#include "target_rules.h"

namespace Engine::Core {
class World;
class AttackComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Systems::FormationCombat {
struct ContactGeometry;
} // namespace Game::Systems::FormationCombat

namespace Game::Systems::Combat {

struct CandidateRecord {
  std::uint32_t stamp{0};
  Engine::Core::EntityID id{0};
  Engine::Core::Entity* entity{nullptr};
  int owner_id{0};
  bool is_building{false};
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

struct GuardReach {
  float center_x{0.0F};
  float center_z{0.0F};
  float radius{0.0F};
};

auto guard_post_of(const Engine::Core::Entity* entity) -> std::optional<QVector3D>;

auto guard_reach_of(const Engine::Core::Entity* entity) -> std::optional<GuardReach>;

auto within_guard_reach(const Engine::Core::Entity* entity,
                        float x,
                        float z,
                        float margin = 0.0F) -> bool;

auto guard_answer_fire_margin(const Engine::Core::Entity* aggressor) -> float;

enum class GuardReachRule : std::uint8_t {
  Strict,
  AnswersFire,
};

auto within_guard_reach(const Engine::Core::Entity* entity,
                        const Engine::Core::Entity* target,
                        GuardReachRule rule = GuardReachRule::Strict) -> bool;

auto is_returning_to_guard_post(const Engine::Core::Entity* entity) -> bool;

void send_guard_home(Engine::Core::World& world,
                     Engine::Core::Entity* entity,
                     float arrival_threshold = -1.0F);

[[nodiscard]] auto is_infantry_spawn(Game::Units::SpawnType type) noexcept -> bool;

[[nodiscard]] auto is_melee_mode(const Engine::Core::AttackComponent* attack) -> bool;

[[nodiscard]] auto is_ranged_mode(const Engine::Core::AttackComponent* attack) -> bool;

[[nodiscard]] auto in_rts_melee_lock(const Engine::Core::Entity* entity) -> bool;

auto combat_radius(Engine::Core::Entity* entity) -> float;

auto is_in_range(Engine::Core::Entity* attacker,
                 Engine::Core::Entity* target,
                 float range) -> bool;

auto elephant_formation_penetration_distance(
    const Engine::Core::Entity& attacker,
    const Engine::Core::Entity& target,
    const FormationCombat::ContactGeometry& geometry) -> std::optional<float>;

auto melee_contact_reached(const Engine::Core::Entity& attacker,
                           const Engine::Core::Entity& target,
                           const FormationCombat::ContactGeometry& geometry) -> bool;

auto structure_separates_positions(const QVector3D& from, const QVector3D& to) -> bool;

auto structure_separates_combatants(Engine::Core::Entity* attacker,
                                    Engine::Core::Entity* target) -> bool;

inline constexpr float k_min_bypass_clearance = 0.6F;

auto melee_bypass_destination(const QVector3D& attacker_position,
                              const QVector3D& target_position,
                              float standoff_distance,
                              float clearance_radius) -> std::optional<QVector3D>;

inline constexpr float k_opportunity_walk_around_detour = 8.0F;
inline constexpr float k_answering_walk_around_detour = 16.0F;

auto melee_walk_around_length(Engine::Core::Entity* attacker,
                              Engine::Core::Entity* target) -> std::optional<float>;

auto melee_walled_off_from(Engine::Core::Entity* attacker,
                           Engine::Core::Entity* target,
                           float allowed_detour = k_opportunity_walk_around_detour)
    -> bool;

auto suppresses_opportunistic_combat(Engine::Core::Entity* unit) -> bool;

auto combat_role_of(const Engine::Core::Entity* entity) -> Game::Units::CombatRole;

auto auto_acquires_targets(Engine::Core::Entity* entity) -> bool;

auto answers_threat_alerts(Engine::Core::Entity* entity) -> bool;

auto pursues_targets(const Engine::Core::Entity* entity) -> bool;

auto opens_fire_without_closing(const Engine::Core::Entity* entity) -> bool;

auto acquisition_range(Engine::Core::Entity* entity) -> float;

auto is_unit_idle(Engine::Core::Entity* unit) -> bool;

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
                        const TargetFilter& accept = {},
                        Engine::Core::Entity** nearest_considered = nullptr,
                        TargetQuery query = {
                            .intent = EngagementIntent::AutoAcquired,
                            .allow_buildings = false}) -> Engine::Core::Entity*;

} // namespace Game::Systems::Combat
