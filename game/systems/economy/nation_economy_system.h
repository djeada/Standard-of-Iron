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

class NationEconomySystem : public Engine::Core::System {
public:
  static constexpr float k_payday_seconds = 60.0F;
  static constexpr float k_unpaid_morale_loss = 20.0F;

  static constexpr float k_home_ground_radius = 30.0F;

  static constexpr float k_rank_health_bonus = 0.15F;
  static constexpr float k_battle_over_seconds = 20.0F;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto phase() const -> Engine::Core::SystemPhase override {
    return Engine::Core::SystemPhase::Economy;
  }

  [[nodiscard]] auto elapsed_seconds() const -> float { return m_elapsed; }

  [[nodiscard]] auto seconds_to_next_levy(const Engine::Core::World& world,
                                          int owner_id) const -> float;

  [[nodiscard]] static auto upkeep_per_minute(const Engine::Core::World& world,
                                              int owner_id) -> int;
  [[nodiscard]] static auto trade_income_per_minute(const Engine::Core::World& world,
                                                    int owner_id) -> int;

  [[nodiscard]] auto serialize_state() const -> QJsonObject;
  void restore_state(const QJsonObject& state);

  static void pay_upkeep(Engine::Core::World& world, int owner_id);
  static void pay_trade_income(Engine::Core::World& world, int owner_id);
  static auto call_levy(Engine::Core::World& world, int owner_id) -> int;

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
