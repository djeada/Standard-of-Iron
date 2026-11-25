#pragma once

#include <QJsonObject>

#include <unordered_map>
#include <unordered_set>

#include "core/entity_id.h"
#include "core/system.h"

namespace Engine::Core {
class World;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Systems {

// The economies that make Rome and Carthage play differently
// (NATION_ASYMMETRY.md), driven by each nation's "economy" block:
//  - Carthage pays its mercenary bands every minute; an unpaid band fights
//    worse until it is paid again (decision 2: no desertion).
//  - Carthage's Emporion (its marketplace) earns gold every minute, more for
//    the farms around it.
//  - Rome's levy (dilectus): every interval each barracks gains men for every
//    standing home near it.
//  - Rome's replacement: a squad destroyed in the field sends a share of its
//    men back to the nearest barracks.
class NationEconomySystem : public Engine::Core::System {
public:
  static constexpr float k_payday_seconds = 60.0F;
  static constexpr float k_unpaid_morale_loss = 20.0F;
  // A squad that falls this close to one of its own buildings died defending
  // home, not in the field, and is not replaced.
  static constexpr float k_home_ground_radius = 30.0F;
  // Veterancy: health a promoted squad gains per rank, and how long a squad
  // must be out of the fight before the battle counts as one it survived.
  static constexpr float k_rank_health_bonus = 0.15F;
  static constexpr float k_battle_over_seconds = 20.0F;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto phase() const -> Engine::Core::SystemPhase override {
    return Engine::Core::SystemPhase::Economy;
  }

  [[nodiscard]] auto elapsed_seconds() const -> float { return m_elapsed; }

  // Seconds until the owner's next levy, or a negative number when its
  // nation has none.
  [[nodiscard]] auto seconds_to_next_levy(const Engine::Core::World& world,
                                          int owner_id) const -> float;

  // Gold the owner's bands cost a minute, and what its trade houses earn.
  [[nodiscard]] static auto upkeep_per_minute(const Engine::Core::World& world,
                                              int owner_id) -> int;
  [[nodiscard]] static auto trade_income_per_minute(const Engine::Core::World& world,
                                                    int owner_id) -> int;

  [[nodiscard]] auto serialize_state() const -> QJsonObject;
  void restore_state(const QJsonObject& state);

  // Runs one payday or levy for an owner now; tests and the AI observatory
  // use them to step the economy without waiting.
  static void pay_upkeep(Engine::Core::World& world, int owner_id);
  static void pay_trade_income(Engine::Core::World& world, int owner_id);
  static auto call_levy(Engine::Core::World& world, int owner_id) -> int;
  // Raises a squad to `rank`, scaling its health; returns whether it rose.
  static auto promote(Engine::Core::UnitComponent& unit, int rank) -> bool;

private:
  void refund_fallen_squads(Engine::Core::World& world);
  void heal_in_camps(Engine::Core::World& world, float seconds);
  void track_veterans(Engine::Core::World& world, float seconds);

  struct BattleWatch {
    bool fighting = false;
    float quiet = 0.0F;
    int last_health = 0;
  };
  std::unordered_map<Engine::Core::EntityID, BattleWatch> m_battles;
  float m_slow_tick{0.0F};

  float m_elapsed{0.0F};
  float m_refund_scan{0.0F};
  std::unordered_set<Engine::Core::EntityID> m_refunded;
};

} // namespace Game::Systems
