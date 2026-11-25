#include "nation_economy_system.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

#include "audio/cue_ids.h"
#include "core/ambient_session.h"
#include "core/component_combat.h"
#include "core/component_core.h"
#include "core/component_economy.h"
#include "core/component_structures.h"
#include "core/event_manager.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "nation_recruitment.h"
#include "systems/nation_registry.h"
#include "systems/player_feedback.h"
#include "systems/player_resource_registry.h"
#include "systems/troop_profile_service.h"
#include "units/troop_type.h"

namespace Game::Systems {

namespace {

auto position_of(const Engine::Core::World& world,
                 Engine::Core::EntityID id) -> const Engine::Core::TransformComponent* {
  return world.try_get<Engine::Core::TransformComponent>(id);
}

auto planar_distance(const Engine::Core::TransformComponent& a,
                     const Engine::Core::TransformComponent& b) -> float {
  return std::hypot(a.position.x - b.position.x, a.position.z - b.position.z);
}

auto standing_buildings(const Engine::Core::World& world,
                        int owner_id,
                        Game::Units::SpawnType type)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> found;
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    if (unit.owner_id == owner_id && unit.spawn_type == type && unit.health > 0) {
      found.push_back(id);
    }
  }
  std::sort(found.begin(), found.end());
  return found;
}

auto count_near(const Engine::Core::World& world,
                const std::vector<Engine::Core::EntityID>& candidates,
                const Engine::Core::TransformComponent& centre,
                float radius) -> int {
  int count = 0;
  for (const auto id : candidates) {
    const auto* transform = position_of(world, id);
    if (transform != nullptr && planar_distance(*transform, centre) <= radius) {
      ++count;
    }
  }
  return count;
}

auto active_owners(const Engine::Core::World& world) -> std::set<int> {
  std::set<int> owners;
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    (void)id;
    if (unit.health > 0 && !Game::Core::is_neutral_owner(unit.owner_id)) {
      owners.insert(unit.owner_id);
    }
  }
  return owners;
}

void announce(int owner_id, const QString& text, const char* cue) {
  auto& events = Engine::Core::EventManager::instance();
  events.publish(Engine::Core::MissionAnnouncementEvent::for_owner(owner_id, text));
  if (cue != nullptr) {
    events.publish(Engine::Core::AudioCueEvent::for_owner(owner_id, cue));
  }
}

auto crossed(float before, float after, float interval) -> bool {
  if (interval <= 0.0F) {
    return false;
  }
  return std::floor(after / interval) > std::floor(before / interval);
}

} // namespace

auto NationEconomySystem::upkeep_per_minute(const Engine::Core::World& world,
                                            int owner_id) -> int {
  int total = 0;
  for (auto [id, contract] : world.view<Engine::Core::MercenaryContractComponent>()) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
      total += std::max(0, contract.upkeep_per_minute);
    }
  }
  return total;
}

auto NationEconomySystem::trade_income_per_minute(const Engine::Core::World& world,
                                                  int owner_id) -> int {
  const auto* nation = owner_nation(world, owner_id);
  if (nation == nullptr || nation->economy.trade_income_per_minute <= 0) {
    return 0;
  }
  const auto& traits = nation->economy;
  auto markets =
      standing_buildings(world, owner_id, Game::Units::SpawnType::Marketplace);
  if (static_cast<int>(markets.size()) > traits.trade_income_max_buildings) {
    markets.resize(
        static_cast<std::size_t>(std::max(0, traits.trade_income_max_buildings)));
  }
  const auto farms = standing_buildings(world, owner_id, Game::Units::SpawnType::Farm);
  int total = 0;
  for (const auto market : markets) {
    total += traits.trade_income_per_minute;
    if (const auto* centre = position_of(world, market)) {
      total += traits.trade_income_per_farm *
               count_near(world, farms, *centre, traits.trade_income_radius);
    }
  }
  return total;
}

void NationEconomySystem::pay_upkeep(Engine::Core::World& world, int owner_id) {
  const int owed = upkeep_per_minute(world, owner_id);
  if (owed <= 0) {
    return;
  }
  const auto* services = &Game::Session::services_for(world);
  if (services->economy == nullptr) {
    return;
  }
  std::vector<Engine::Core::MercenaryContractComponent*> contracts;
  Engine::Core::EntityID anchor = 0;
  for (auto [id, contract] : world.view<Engine::Core::MercenaryContractComponent>()) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr || unit->owner_id != owner_id || unit->health <= 0) {
      continue;
    }
    contracts.push_back(&contract);
    if (anchor == 0) {
      anchor = id;
    }
  }
  const auto camps =
      standing_buildings(world, owner_id, Game::Units::SpawnType::MercenaryCamp);
  if (!camps.empty()) {
    anchor = camps.front();
  }

  if (services->economy->get(owner_id, ResourceType::Gold) >= owed) {
    ResourceAmounts bill;
    bill.set(ResourceType::Gold, owed);
    spend_resources(owner_id, anchor, bill);
    for (auto* contract : contracts) {
      contract->unpaid = false;
    }
    return;
  }

  bool newly_unpaid = false;
  for (auto [id, contract] : world.view<Engine::Core::MercenaryContractComponent>()) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr || unit->owner_id != owner_id || unit->health <= 0 ||
        contract.unpaid) {
      continue;
    }
    contract.unpaid = true;
    newly_unpaid = true;
    auto* morale = world.try_get<Engine::Core::MoraleComponent>(id);
    if (morale == nullptr) {
      morale = world.emplace<Engine::Core::MoraleComponent>(id);
    }
    if (morale != nullptr) {
      morale->morale -= k_unpaid_morale_loss;
      Engine::Core::refresh_morale_state(*morale);
    }
  }
  if (newly_unpaid) {
    announce(owner_id,
             QCoreApplication::translate(
                 "NationEconomy",
                 "The mercenaries were not paid. They fight without heart until the "
                 "treasury can pay them."),
             Game::Audio::Cue::k_alert_low_resources);
  }
}

void NationEconomySystem::pay_trade_income(Engine::Core::World& world, int owner_id) {
  const auto* nation = owner_nation(world, owner_id);
  if (nation == nullptr || nation->economy.trade_income_per_minute <= 0) {
    return;
  }
  const auto& traits = nation->economy;
  auto markets =
      standing_buildings(world, owner_id, Game::Units::SpawnType::Marketplace);
  if (static_cast<int>(markets.size()) > traits.trade_income_max_buildings) {
    markets.resize(
        static_cast<std::size_t>(std::max(0, traits.trade_income_max_buildings)));
  }
  const auto farms = standing_buildings(world, owner_id, Game::Units::SpawnType::Farm);
  for (const auto market : markets) {
    int income = traits.trade_income_per_minute;
    if (const auto* centre = position_of(world, market)) {
      income += traits.trade_income_per_farm *
                count_near(world, farms, *centre, traits.trade_income_radius);
    }
    grant_resource(owner_id, market, ResourceType::Gold, income);
  }
}

auto NationEconomySystem::call_levy(Engine::Core::World& world, int owner_id) -> int {
  const auto* nation = owner_nation(world, owner_id);
  if (nation == nullptr || nation->economy.levy_men_per_home <= 0) {
    return 0;
  }
  const auto& traits = nation->economy;
  const auto homes = standing_buildings(world, owner_id, Game::Units::SpawnType::Home);
  int raised = 0;
  for (const auto barracks :
       standing_buildings(world, owner_id, Game::Units::SpawnType::Barracks)) {
    auto* production = world.try_get<Engine::Core::ProductionComponent>(barracks);
    const auto* centre = position_of(world, barracks);
    if (production == nullptr || centre == nullptr) {
      continue;
    }
    const int men = traits.levy_men_per_home *
                    count_near(world, homes, *centre, traits.levy_radius);
    raised += grant_manpower(
        owner_id, barracks, *production, men, production->manpower_limit());
  }
  if (raised > 0) {
    announce(owner_id,
             QCoreApplication::translate(
                 "NationEconomy", "Levy day: %1 citizens answer the call to arms.")
                 .arg(raised),
             Game::Audio::Cue::k_alert_reinforcements);
  }
  return raised;
}

auto NationEconomySystem::seconds_to_next_levy(const Engine::Core::World& world,
                                               int owner_id) const -> float {
  const auto* nation = owner_nation(world, owner_id);
  if (nation == nullptr || nation->economy.levy_interval_seconds <= 0.0F ||
      nation->economy.levy_men_per_home <= 0) {
    return -1.0F;
  }
  const float interval = nation->economy.levy_interval_seconds;
  return interval - std::fmod(m_elapsed, interval);
}

void NationEconomySystem::refund_fallen_squads(Engine::Core::World& world) {
  for (auto it = m_refunded.begin(); it != m_refunded.end();) {
    it = world.get_entity(*it) == nullptr ? m_refunded.erase(it) : std::next(it);
  }

  struct Fallen {
    Engine::Core::EntityID id;
    int owner_id;
    int refund;
    float x;
    float z;
  };
  std::vector<Fallen> fallen;
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    if (unit.health > 0 || m_refunded.contains(id) ||
        !Game::Units::is_combatant_spawn(unit.spawn_type) ||
        Game::Units::is_siege_engine_spawn(unit.spawn_type) ||
        Game::Core::is_neutral_owner(unit.owner_id)) {
      continue;
    }
    const auto troop = Game::Units::spawn_typeToTroopType(unit.spawn_type);
    if (!troop.has_value() || Game::Units::is_commander_troop(*troop)) {
      continue;
    }
    m_refunded.insert(id);
    const auto* nation = owner_nation(world, unit.owner_id);
    const auto* transform = position_of(world, id);
    if (nation == nullptr || nation->economy.loss_refund_share <= 0.0F ||
        transform == nullptr) {
      continue;
    }
    const int establishment = unit.squad_strength > 0
                                  ? unit.squad_strength
                                  : TroopProfileService::instance()
                                        .get_profile_ref(unit.nation_id, *troop)
                                        .individuals_per_unit;
    const int refund = static_cast<int>(std::lround(static_cast<float>(establishment) *
                                                    nation->economy.loss_refund_share));
    if (refund > 0) {
      fallen.push_back(Fallen{
          id, unit.owner_id, refund, transform->position.x, transform->position.z});
    }
  }

  for (const auto& loss : fallen) {
    bool at_home = false;
    Engine::Core::EntityID nearest = 0;
    float best = std::numeric_limits<float>::max();
    for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
      if (unit.owner_id != loss.owner_id || unit.health <= 0 ||
          !Game::Units::is_building_spawn(unit.spawn_type)) {
        continue;
      }
      const auto* transform = position_of(world, id);
      if (transform == nullptr) {
        continue;
      }
      const float distance =
          std::hypot(transform->position.x - loss.x, transform->position.z - loss.z);
      if (distance <= k_home_ground_radius) {
        at_home = true;
        break;
      }
      if (unit.spawn_type == Game::Units::SpawnType::Barracks && distance < best &&
          world.try_get<Engine::Core::ProductionComponent>(id) != nullptr) {
        best = distance;
        nearest = id;
      }
    }
    if (at_home || nearest == 0) {
      continue;
    }
    auto* production = world.try_get<Engine::Core::ProductionComponent>(nearest);
    (void)grant_manpower(
        loss.owner_id, nearest, *production, loss.refund, production->manpower_limit());
  }
}

void NationEconomySystem::heal_in_camps(Engine::Core::World& world, float seconds) {
  for (auto [camp_id, camp] : world.view<Engine::Core::UnitComponent>()) {
    if (camp.spawn_type != Game::Units::SpawnType::Castra || camp.health <= 0) {
      continue;
    }
    const auto* nation = owner_nation(world, camp.owner_id);
    const auto* centre = position_of(world, camp_id);
    if (nation == nullptr || centre == nullptr ||
        nation->economy.camp_heal_per_second <= 0.0F) {
      continue;
    }
    for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
      if (unit.owner_id != camp.owner_id || unit.health <= 0 ||
          unit.health >= unit.max_health ||
          Game::Units::is_building_spawn(unit.spawn_type)) {
        continue;
      }
      const auto* transform = position_of(world, id);
      if (transform == nullptr ||
          planar_distance(*transform, *centre) > nation->economy.camp_heal_radius) {
        continue;
      }
      const int amount = std::max(1,
                                  static_cast<int>(std::lround(
                                      static_cast<float>(unit.max_health) *
                                      nation->economy.camp_heal_per_second * seconds)));
      (void)restore_health(unit.owner_id, id, unit, amount, unit.max_health);
    }
  }
}

auto NationEconomySystem::promote(Engine::Core::UnitComponent& unit, int rank) -> bool {
  if (rank <= static_cast<int>(unit.rank)) {
    return false;
  }
  const float before = 1.0F + k_rank_health_bonus * static_cast<float>(unit.rank);
  const float after = 1.0F + k_rank_health_bonus * static_cast<float>(rank);
  const float scale = after / before;
  unit.max_health =
      static_cast<int>(std::lround(static_cast<float>(unit.max_health) * scale));
  unit.health =
      std::min(unit.max_health,
               static_cast<int>(std::lround(static_cast<float>(unit.health) * scale)));
  unit.rank = static_cast<std::uint8_t>(rank);
  return true;
}

void NationEconomySystem::track_veterans(Engine::Core::World& world, float seconds) {
  for (auto it = m_battles.begin(); it != m_battles.end();) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(it->first);
    it = (unit == nullptr || unit->health <= 0) ? m_battles.erase(it) : std::next(it);
  }
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    if (unit.health <= 0 || Game::Core::is_neutral_owner(unit.owner_id)) {
      continue;
    }
    const auto* nation = owner_nation(world, unit.owner_id);
    if (nation == nullptr || !nation->economy.veteran_troop.has_value() ||
        Game::Units::spawn_typeFromTroopType(*nation->economy.veteran_troop) !=
            unit.spawn_type) {
      continue;
    }
    auto& watch = m_battles[id];
    const auto* target = world.try_get<Engine::Core::AttackTargetComponent>(id);
    const bool engaged = (target != nullptr && target->target_id != 0) ||
                         (watch.last_health > 0 && unit.health < watch.last_health);
    watch.last_health = unit.health;
    if (engaged) {
      watch.fighting = true;
      watch.quiet = 0.0F;
      continue;
    }
    if (!watch.fighting) {
      continue;
    }
    watch.quiet += seconds;
    if (watch.quiet < k_battle_over_seconds) {
      continue;
    }
    watch.fighting = false;
    watch.quiet = 0.0F;
    if (unit.battles_survived < 255) {
      ++unit.battles_survived;
    }
    const int battles = unit.battles_survived;
    const int rank = battles >= nation->economy.battles_for_second_rank  ? 2
                     : battles >= nation->economy.battles_for_first_rank ? 1
                                                                         : 0;
    if (promote(unit, rank)) {
      announce(unit.owner_id,
               rank == 1 ? QCoreApplication::translate(
                               "NationEconomy",
                               "A cohort of hastati has come through two battles. "
                               "They fight as principes now.")
                         : QCoreApplication::translate(
                               "NationEconomy",
                               "A cohort of principes has seen enough battles to "
                               "stand with the veterans."),
               Game::Audio::Cue::k_alert_objective_complete);
    }
  }
}

void NationEconomySystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }
  const float before = m_elapsed;
  m_elapsed += delta_time;

  m_refund_scan += delta_time;
  constexpr float k_refund_scan_seconds = 0.25F;
  if (m_refund_scan >= k_refund_scan_seconds) {
    m_refund_scan = 0.0F;
    refund_fallen_squads(*world);
  }

  m_slow_tick += delta_time;
  constexpr float k_slow_tick_seconds = 1.0F;
  if (m_slow_tick >= k_slow_tick_seconds) {
    heal_in_camps(*world, m_slow_tick);
    track_veterans(*world, m_slow_tick);
    m_slow_tick = 0.0F;
  }

  const bool payday = crossed(before, m_elapsed, k_payday_seconds);
  for (const int owner : active_owners(*world)) {
    if (payday) {
      pay_trade_income(*world, owner);
      pay_upkeep(*world, owner);
    }
    const auto* nation = owner_nation(*world, owner);
    if (nation != nullptr &&
        crossed(before, m_elapsed, nation->economy.levy_interval_seconds)) {
      (void)call_levy(*world, owner);
    }
  }
}

auto NationEconomySystem::serialize_state() const -> QJsonObject {
  QJsonObject state;
  state["elapsed"] = static_cast<double>(m_elapsed);
  return state;
}

void NationEconomySystem::restore_state(const QJsonObject& state) {
  m_elapsed = static_cast<float>(state.value("elapsed").toDouble(0.0));
  m_refunded.clear();
}

} // namespace Game::Systems
