#include "ai_attack_wave.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "../../units/spawn_type.h"
#include "../../units/troop_type.h"
#include "ai_doctrine_catalog.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

auto is_commander_contact(const ContactSnapshot& contact) -> bool {
  const auto troop = Game::Units::spawn_typeToTroopType(contact.spawn_type);
  return troop.has_value() && Game::Units::is_commander_troop(*troop);
}

auto matches_target(const ContactSnapshot& contact, DoctrineTarget target) -> bool {
  switch (target) {
  case DoctrineTarget::Army:
    return !contact.is_building && !is_commander_contact(contact);
  case DoctrineTarget::Barracks:
    return contact.spawn_type == Game::Units::SpawnType::Barracks;
  case DoctrineTarget::Economy:
    return contact.spawn_type == Game::Units::SpawnType::Builder ||
           contact.spawn_type == Game::Units::SpawnType::Home ||
           contact.spawn_type == Game::Units::SpawnType::Marketplace;
  case DoctrineTarget::Commander:
    return is_commander_contact(contact);
  case DoctrineTarget::Any:
    return true;
  }
  return true;
}

auto default_target_priority() -> const std::vector<DoctrineTarget>& {
  static const std::vector<DoctrineTarget> value{DoctrineTarget::Army,
                                                 DoctrineTarget::Barracks,
                                                 DoctrineTarget::Economy,
                                                 DoctrineTarget::Any};
  return value;
}

auto target_priority_for(const AIContext& context)
    -> const std::vector<DoctrineTarget>& {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->wave.target_priority.empty()) {
    return default_target_priority();
  }
  return doctrine->wave.target_priority;
}

auto regroup_seconds_for(const AIContext& context) -> float {
  const auto* doctrine = context.strategy_config.doctrine;
  return doctrine != nullptr ? doctrine->wave.regroup_seconds : 25.0F;
}

auto spent_fraction_for(const AIContext& context) -> float {
  const auto* doctrine = context.strategy_config.doctrine;
  return doctrine != nullptr ? doctrine->wave.spent_fraction : 0.35F;
}

auto nearest_matching(const std::vector<ContactSnapshot>& contacts,
                      DoctrineTarget target_kind,
                      float from_x,
                      float from_z) -> const ContactSnapshot* {
  const ContactSnapshot* best = nullptr;
  float best_distance_sq = std::numeric_limits<float>::infinity();
  for (const auto& contact : contacts) {
    if (contact.health <= 0 || !matches_target(contact, target_kind)) {
      continue;
    }
    const float distance_sq =
        distance_squared(contact.pos_x, 0.0F, contact.pos_z, from_x, 0.0F, from_z);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best = &contact;
    }
  }
  return best;
}

auto select_wave_target(const AISnapshot& snapshot,
                        const AIContext& context,
                        float from_x,
                        float from_z) -> const ContactSnapshot* {
  for (const auto target_kind : target_priority_for(context)) {
    if (const auto* seen =
            nearest_matching(snapshot.visible_enemies, target_kind, from_x, from_z)) {
      return seen;
    }
  }

  for (const auto target_kind : target_priority_for(context)) {
    if (const auto* known = nearest_matching(
            snapshot.strategic_objectives, target_kind, from_x, from_z)) {
      return known;
    }
  }
  return nullptr;
}

auto find_contact(const AISnapshot& snapshot,
                  Engine::Core::EntityID id) -> const ContactSnapshot* {
  for (const auto& contact : snapshot.visible_enemies) {
    if (contact.id == id && contact.health > 0) {
      return &contact;
    }
  }
  for (const auto& objective : snapshot.strategic_objectives) {
    if (objective.id == id && objective.health > 0) {
      return &objective;
    }
  }
  return nullptr;
}

auto find_friendly(const AISnapshot& snapshot,
                   Engine::Core::EntityID id) -> const EntitySnapshot* {
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.id == id) {
      return &entity;
    }
  }
  return nullptr;
}

auto marches_with_a_wave(const EntitySnapshot& entity) -> bool {

  return marches_with_the_army(entity) &&
         entity.spawn_type != Game::Units::SpawnType::Civilian &&
         entity.spawn_type != Game::Units::SpawnType::Healer;
}

auto committable_units(const AISnapshot& snapshot,
                       const AIContext& context) -> std::vector<const EntitySnapshot*> {
  std::vector<const EntitySnapshot*> result;
  result.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!marches_with_a_wave(entity) || entity.is_assault ||
        is_harass_unit(entity.id, context)) {
      continue;
    }
    result.push_back(&entity);
  }
  return result;
}

} // namespace

auto wave_size_for(const AIContext& context) -> int {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine != nullptr) {
    return std::max(1, doctrine->wave.size);
  }

  return std::max(1, context.strategy_config.proactive_attack_size);
}

namespace {

auto required_wave_size(const AIContext& context, float game_time) -> int {
  const int authored = wave_size_for(context);

  constexpr float k_opening_grace_seconds = 420.0F;
  constexpr float k_patience_seconds = 300.0F;
  constexpr float k_relent_seconds = 120.0F;
  constexpr int k_smallest_wave = 4;

  const float waited =
      game_time - std::max(k_opening_grace_seconds, context.wave.ended_at);
  if (waited <= k_patience_seconds) {
    return authored;
  }
  const int relented =
      static_cast<int>((waited - k_patience_seconds) / k_relent_seconds) + 1;
  return std::max(k_smallest_wave, authored - relented);
}

} // namespace

auto garrison_target_for(const AIContext& context,
                         int combat_unit_count,
                         int keep_free) -> int {
  const auto* doctrine = context.strategy_config.doctrine;
  int minimum = 0;
  float fraction = 0.0F;
  if (doctrine != nullptr) {
    minimum = doctrine->garrison.minimum_units;
    fraction = doctrine->garrison.fraction;
  } else {
    minimum = std::max(0, context.strategy_config.reserve_units);
  }

  const int by_fraction =
      static_cast<int>(fraction * static_cast<float>(combat_unit_count));
  const int wanted = std::max(minimum, by_fraction);

  const int ceiling = std::max(0, combat_unit_count - std::max(1, keep_free));
  return std::clamp(wanted, 0, ceiling);
}

void update_attack_wave(const AISnapshot& snapshot, AIContext& context) {
  auto& wave = context.wave;
  const auto candidates = committable_units(snapshot, context);

  const int required = required_wave_size(context, snapshot.game_time);

  const int garrison_target =
      garrison_target_for(context, static_cast<int>(candidates.size()), required);
  std::vector<const EntitySnapshot*> by_distance = candidates;
  if (context.has_base_anchor) {
    std::sort(by_distance.begin(),
              by_distance.end(),
              [&](const EntitySnapshot* lhs, const EntitySnapshot* rhs) {
                return distance_squared(lhs->pos_x,
                                        0.0F,
                                        lhs->pos_z,
                                        context.base_pos_x,
                                        0.0F,
                                        context.base_pos_z) <
                       distance_squared(rhs->pos_x,
                                        0.0F,
                                        rhs->pos_z,
                                        context.base_pos_x,
                                        0.0F,
                                        context.base_pos_z);
              });
  }
  context.garrison_unit_ids.clear();
  for (int i = 0; i < garrison_target && i < static_cast<int>(by_distance.size());
       ++i) {
    context.garrison_unit_ids.push_back(by_distance[static_cast<std::size_t>(i)]->id);
  }
  const std::unordered_set<Engine::Core::EntityID> garrison(
      context.garrison_unit_ids.begin(), context.garrison_unit_ids.end());

  if (wave.committed) {

    std::vector<Engine::Core::EntityID> survivors;
    survivors.reserve(wave.members.size());
    float centre_x = 0.0F;
    float centre_z = 0.0F;
    for (const auto id : wave.members) {
      const auto* entity = find_friendly(snapshot, id);
      if (entity == nullptr || entity->health <= 0) {
        continue;
      }
      survivors.push_back(id);
      centre_x += entity->pos_x;
      centre_z += entity->pos_z;
    }
    wave.members = std::move(survivors);

    const int remaining = static_cast<int>(wave.members.size());

    const int spent_threshold =
        std::max(2,
                 static_cast<int>(
                     std::ceil(spent_fraction_for(context) *
                               static_cast<float>(std::max(1, wave.initial_size)))));
    if (remaining <= spent_threshold) {
      wave.committed = false;
      wave.members.clear();
      wave.target_id = 0;
      wave.ended_at = snapshot.game_time;
      return;
    }

    centre_x /= static_cast<float>(remaining);
    centre_z /= static_cast<float>(remaining);

    const ContactSnapshot* target = find_contact(snapshot, wave.target_id);
    if (target == nullptr) {
      target = select_wave_target(snapshot, context, centre_x, centre_z);
      if (target == nullptr) {
        wave.committed = false;
        wave.members.clear();
        wave.target_id = 0;
        wave.ended_at = snapshot.game_time;
        return;
      }
      wave.target_id = target->id;
    }
    wave.target_x = target->pos_x;
    wave.target_z = target->pos_z;
    return;
  }

  if (snapshot.game_time - wave.ended_at < regroup_seconds_for(context)) {
    return;
  }

  std::vector<const EntitySnapshot*> available;
  available.reserve(candidates.size());
  for (const auto* entity : candidates) {
    if (garrison.contains(entity->id)) {
      continue;
    }
    available.push_back(entity);
  }

  if (static_cast<int>(available.size()) < required) {
    return;
  }

  float centre_x = 0.0F;
  float centre_z = 0.0F;
  for (const auto* entity : available) {
    centre_x += entity->pos_x;
    centre_z += entity->pos_z;
  }
  centre_x /= static_cast<float>(available.size());
  centre_z /= static_cast<float>(available.size());

  const ContactSnapshot* target =
      select_wave_target(snapshot, context, centre_x, centre_z);
  if (target == nullptr) {
    return;
  }

  wave.members.clear();
  wave.members.reserve(available.size());
  for (const auto* entity : available) {
    wave.members.push_back(entity->id);
  }
  wave.initial_size = static_cast<int>(wave.members.size());
  wave.target_id = target->id;
  wave.target_x = target->pos_x;
  wave.target_z = target->pos_z;
  wave.committed = true;
  wave.committed_at = snapshot.game_time;
}

auto wave_force_units(const AISnapshot& snapshot,
                      const AIContext& context) -> std::vector<const EntitySnapshot*> {
  std::vector<const EntitySnapshot*> result;
  if (!context.wave.committed) {
    return result;
  }
  result.reserve(context.wave.members.size());
  for (const auto id : context.wave.members) {
    if (const auto* entity = find_friendly(snapshot, id);
        entity != nullptr && entity->health > 0) {
      result.push_back(entity);
    }
  }
  return result;
}

} // namespace Game::Systems::AI
