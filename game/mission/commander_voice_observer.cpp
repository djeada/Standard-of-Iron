#include "commander_voice_observer.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <utility>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_types.h"
#include "game/units/spawn_type.h"

namespace Game::Mission {

auto AiSystemAttackPlanSource::attack_plan(int owner_id) const
    -> std::optional<AttackPlan> {
  if (m_ai == nullptr) {
    return std::nullopt;
  }
  const auto* context = m_ai->plan_for(owner_id);
  if (context == nullptr) {
    return std::nullopt;
  }
  return AttackPlan{.committed = context->wave.committed,
                    .committed_at = context->wave.committed_at,
                    .target_id = context->wave.target_id};
}

CommanderVoiceObserver::~CommanderVoiceObserver() {
  unsubscribe();
}

void CommanderVoiceObserver::configure(std::vector<int> watched_owners,
                                       int local_owner_id,
                                       Tuning tuning) {
  clear();
  m_owners = std::move(watched_owners);
  std::sort(m_owners.begin(), m_owners.end());
  m_owners.erase(std::unique(m_owners.begin(), m_owners.end()), m_owners.end());
  m_local_owner_id = local_owner_id;
  m_tuning = tuning;
  for (const int owner : m_owners) {
    m_states.emplace(owner, OwnerState{});
  }
  if (!m_owners.empty()) {
    subscribe();
  }
}

void CommanderVoiceObserver::clear() {
  unsubscribe();
  m_owners.clear();
  m_states.clear();
  m_contacted.clear();
  m_elapsed = 0.0F;
  m_poll_accumulator = 0.0F;
  const std::lock_guard<std::mutex> lock(m_inbox_mutex);
  m_building_hits.clear();
  m_contacts.clear();
  m_deaths.clear();
}

void CommanderVoiceObserver::subscribe() {
  m_building_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>(
          [this](const auto& event) {
            const std::lock_guard<std::mutex> lock(m_inbox_mutex);
            m_building_hits.push_back({.owner_id = event.owner_id,
                                       .attacker_owner_id = event.attacker_owner_id,
                                       .damage = event.damage});
          });
  m_combat_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>(
          [this](const auto& event) {
            const std::lock_guard<std::mutex> lock(m_inbox_mutex);
            m_contacts.push_back({.attacker_owner_id = event.attacker_owner_id,
                                  .target_owner_id = event.target_owner_id});
          });
  m_death_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const auto& event) {
            const std::lock_guard<std::mutex> lock(m_inbox_mutex);
            m_deaths.push_back(
                {.owner_id = event.owner_id,
                 .killer_owner_id = event.killer_owner_id,
                 .is_building = Game::Units::is_building_spawn(event.spawn_type)});
          });
}

void CommanderVoiceObserver::unsubscribe() {
  m_building_subscription.unsubscribe();
  m_combat_subscription.unsubscribe();
  m_death_subscription.unsubscribe();
}

auto CommanderVoiceObserver::state_for(int owner_id) -> OwnerState* {
  const auto it = m_states.find(owner_id);
  return it == m_states.end() ? nullptr : &it->second;
}

void CommanderVoiceObserver::update(Engine::Core::World& world,
                                    const AttackPlanSource* plans,
                                    float delta_time) {
  if (m_owners.empty()) {
    return;
  }
  m_elapsed += std::max(0.0F, delta_time);
  drain_inboxes();

  m_poll_accumulator += std::max(0.0F, delta_time);
  if (m_poll_accumulator >= m_tuning.poll_interval_seconds) {
    m_poll_accumulator = 0.0F;
    poll(world, plans);
  }
}

void CommanderVoiceObserver::drain_inboxes() {
  std::vector<BuildingHitFact> hits;
  std::vector<ContactFact> contacts;
  std::vector<DeathFact> deaths;
  {
    const std::lock_guard<std::mutex> lock(m_inbox_mutex);
    hits.swap(m_building_hits);
    contacts.swap(m_contacts);
    deaths.swap(m_deaths);
  }
  for (const auto& hit : hits) {
    note_building_hit(hit);
  }
  for (const auto& contact : contacts) {
    note_contact(contact);
  }
  for (const auto& death : deaths) {
    note_death(death);
  }
}

void CommanderVoiceObserver::note_building_hit(const BuildingHitFact& fact) {
  auto* state = state_for(fact.owner_id);
  if (state == nullptr || fact.attacker_owner_id < 0 ||
      fact.attacker_owner_id == fact.owner_id || fact.damage <= 0) {
    return;
  }
  auto& assault = state->assaults[fact.attacker_owner_id];
  if (assault.hot &&
      m_elapsed - assault.last_hit_at > m_tuning.under_attack_cooloff_seconds) {
    assault.hot = false;
    assault.hits.clear();
  }
  assault.last_hit_at = m_elapsed;
  assault.hits.push_back({.at = m_elapsed, .damage = fact.damage});
  while (!assault.hits.empty() &&
         m_elapsed - assault.hits.front().at > m_tuning.under_attack_window_seconds) {
    assault.hits.pop_front();
  }
  if (assault.hot) {
    return;
  }
  int damage = 0;
  for (const auto& hit : assault.hits) {
    damage += hit.damage;
  }
  if (damage >= m_tuning.under_attack_damage ||
      static_cast<int>(assault.hits.size()) >= m_tuning.under_attack_hits) {
    assault.hot = true;
    Engine::Core::EventManager::instance().publish(
        Engine::Core::OwnerUnderAttackEvent(fact.owner_id, fact.attacker_owner_id));
  }
}

void CommanderVoiceObserver::note_contact(const ContactFact& fact) {
  const int a = fact.attacker_owner_id;
  const int b = fact.target_owner_id;
  if (a < 0 || b < 0 || a == b) {
    return;
  }
  const bool a_watched = state_for(a) != nullptr;
  const bool b_watched = state_for(b) != nullptr;
  if (!a_watched && !b_watched) {
    return;
  }
  const auto key = std::make_pair(std::min(a, b), std::max(a, b));
  if (!m_contacted.insert(key).second) {
    return;
  }

  int owner = a;
  int other = b;
  if (b != m_local_owner_id && a == m_local_owner_id) {
    owner = b;
    other = a;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::OwnersFirstContactEvent(owner, other));
}

void CommanderVoiceObserver::note_death(const DeathFact& fact) {
  auto* state = state_for(fact.owner_id);
  if (state == nullptr) {
    return;
  }
  if (fact.killer_owner_id >= 0 && fact.killer_owner_id != fact.owner_id) {
    state->last_killer_owner_id = fact.killer_owner_id;
  }
  if (fact.is_building) {
    return;
  }
  state->deaths.push_back({.at = m_elapsed, .killer_owner_id = fact.killer_owner_id});
  while (!state->deaths.empty() &&
         m_elapsed - state->deaths.front().at > m_tuning.losses_window_seconds) {
    state->deaths.pop_front();
  }
  if (m_elapsed - state->losses_fired_at < m_tuning.losses_rearm_seconds) {
    return;
  }
  const int threshold =
      std::max(m_tuning.losses_minimum,
               static_cast<int>(static_cast<float>(state->peak_units) *
                                m_tuning.losses_share_of_peak));
  const int losses = static_cast<int>(state->deaths.size());
  if (losses < threshold) {
    return;
  }
  std::map<int, int> killers;
  for (const auto& death : state->deaths) {
    if (death.killer_owner_id >= 0 && death.killer_owner_id != fact.owner_id) {
      ++killers[death.killer_owner_id];
    }
  }
  int killer = -1;
  int best = 0;
  for (const auto& [owner, count] : killers) {
    if (count > best) {
      best = count;
      killer = owner;
    }
  }
  state->losses_fired_at = m_elapsed;
  state->deaths.clear();
  Engine::Core::EventManager::instance().publish(
      Engine::Core::OwnerHeavyLossesEvent(fact.owner_id, killer, losses));
}

void CommanderVoiceObserver::poll(Engine::Core::World& world,
                                  const AttackPlanSource* plans) {
  struct Count {
    int units = 0;
    int structures = 0;
  };
  std::map<int, Count> counts;
  for (const int owner : m_owners) {
    counts.emplace(owner, Count{});
  }
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    const auto it = counts.find(unit.owner_id);
    if (it == counts.end() || unit.health <= 0) {
      continue;
    }
    if (Game::Units::is_building_spawn(unit.spawn_type)) {
      ++it->second.structures;
    } else if (!Game::Units::is_wildlife_spawn(unit.spawn_type)) {
      ++it->second.units;
    }
  }

  auto& bus = Engine::Core::EventManager::instance();
  for (const int owner : m_owners) {
    auto& state = m_states[owner];
    const auto& count = counts[owner];
    state.peak_units = std::max(state.peak_units, count.units);
    state.peak_structures = std::max(state.peak_structures, count.structures);
    if (count.units > 0 || count.structures > 0) {
      state.had_anything = true;
    }

    if (state.had_anything && !state.eliminated && count.units == 0 &&
        count.structures == 0) {
      state.eliminated = true;
      bus.publish(
          Engine::Core::OwnerEliminatedEvent(owner, state.last_killer_owner_id));
      continue;
    }

    const bool armed = state.peak_units >= m_tuning.near_defeat_peak_units;
    const bool last_stand = armed && !state.eliminated &&
                            (count.units <= m_tuning.near_defeat_units ||
                             (count.structures == 0 && state.peak_structures > 0));
    if (last_stand && !state.near_defeat_fired) {
      state.near_defeat_fired = true;
      bus.publish(Engine::Core::OwnerNearDefeatEvent(owner));
    } else if (state.near_defeat_fired &&
               count.units >= m_tuning.near_defeat_units * 2 && count.structures > 0) {
      state.near_defeat_fired = false;
    }

    if (plans == nullptr) {
      continue;
    }
    const auto plan = plans->attack_plan(owner);
    if (!plan.has_value() || !plan->committed ||
        plan->committed_at <= state.last_committed_at) {
      continue;
    }
    state.last_committed_at = plan->committed_at;
    const auto* target = world.try_get<Engine::Core::UnitComponent>(plan->target_id);
    if (target == nullptr || target->owner_id == owner) {
      continue;
    }
    bus.publish(
        Engine::Core::AiAttackLaunchedEvent(owner, target->owner_id, plan->target_id));
  }
}

auto CommanderVoiceObserver::serialize() const -> QJsonObject {
  QJsonObject state;
  state["elapsed"] = static_cast<double>(m_elapsed);
  QJsonArray contacted;
  for (const auto& [a, b] : m_contacted) {
    contacted.append(QJsonArray{a, b});
  }
  state["contacted"] = contacted;
  QJsonObject owners;
  for (const auto& [owner, owner_state] : m_states) {
    QJsonObject entry;
    entry["last_committed_at"] = static_cast<double>(owner_state.last_committed_at);
    entry["losses_fired_at"] = static_cast<double>(owner_state.losses_fired_at);
    entry["peak_units"] = owner_state.peak_units;
    entry["peak_structures"] = owner_state.peak_structures;
    entry["had_anything"] = owner_state.had_anything;
    entry["near_defeat_fired"] = owner_state.near_defeat_fired;
    entry["eliminated"] = owner_state.eliminated;
    entry["last_killer_owner_id"] = owner_state.last_killer_owner_id;
    owners[QString::number(owner)] = entry;
  }
  state["owners"] = owners;
  return state;
}

void CommanderVoiceObserver::restore(const QJsonObject& state) {
  m_elapsed = static_cast<float>(state["elapsed"].toDouble(0.0));
  m_poll_accumulator = 0.0F;
  m_contacted.clear();
  for (const auto value : state["contacted"].toArray()) {
    const QJsonArray pair = value.toArray();
    if (pair.size() == 2) {
      m_contacted.insert({pair[0].toInt(), pair[1].toInt()});
    }
  }
  const QJsonObject owners = state["owners"].toObject();
  for (auto& [owner, owner_state] : m_states) {
    const QJsonObject entry = owners[QString::number(owner)].toObject();
    if (entry.isEmpty()) {
      continue;
    }
    owner_state = OwnerState{};
    owner_state.last_committed_at =
        static_cast<float>(entry["last_committed_at"].toDouble(-1000.0));
    owner_state.losses_fired_at =
        static_cast<float>(entry["losses_fired_at"].toDouble(-1000.0));
    owner_state.peak_units = entry["peak_units"].toInt();
    owner_state.peak_structures = entry["peak_structures"].toInt();
    owner_state.had_anything = entry["had_anything"].toBool();
    owner_state.near_defeat_fired = entry["near_defeat_fired"].toBool();
    owner_state.eliminated = entry["eliminated"].toBool();
    owner_state.last_killer_owner_id = entry["last_killer_owner_id"].toInt(-1);
  }
}

} // namespace Game::Mission
