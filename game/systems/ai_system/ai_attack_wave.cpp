#include "ai_attack_wave.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "../../units/spawn_type.h"
#include "../../units/troop_type.h"
#include "ai_doctrine_catalog.h"
#include "ai_settlement_frame.h"
#include "ai_stall_recovery.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

auto minimum_deployable_strength(const AIContext& context, int required) -> int {
  const auto* doctrine = context.strategy_config.doctrine;
  const int floor = doctrine != nullptr && doctrine->wave.size > 0
                        ? std::max(1, doctrine->wave.size / 2)
                        : std::max(1, required / 2);
  return floor;
}

} // namespace

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
    if (!is_war_contact(contact)) {
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

  return !entity.is_commander && picks_its_own_fights(entity);
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

    if (is_stood_down(entity.id, context, snapshot.game_time)) {
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

constexpr float k_opening_grace_seconds = 420.0F;

auto required_wave_size(const AIContext& context, float game_time) -> int {
  const int authored = wave_size_for(context);

  constexpr float k_patience_seconds = 210.0F;
  constexpr float k_relent_seconds = 90.0F;
  constexpr int k_smallest_wave = 3;

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

namespace {

auto wave_capacity_for(const AIContext& context, int required) -> int {

  constexpr int k_column_multiple = 2;
  return std::max(required, wave_size_for(context)) * k_column_multiple;
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

  const int half_the_army = combat_unit_count / 2;
  const int wanted =
      std::max(minimum, std::min(by_fraction, std::max(minimum, half_the_army)));

  const int ceiling = std::max(0, combat_unit_count - std::max(1, keep_free));

  const int floor_units = std::min(minimum, std::max(0, combat_unit_count - 1));
  return std::clamp(wanted, 0, std::max(ceiling, floor_units));
}

void update_attack_wave(const AISnapshot& snapshot, AIContext& context) {
  auto& wave = context.wave;
  const auto candidates = committable_units(snapshot, context);

  const int required = required_wave_size(context, snapshot.game_time);

  int garrison_target =
      garrison_target_for(context, static_cast<int>(candidates.size()), required);

  constexpr float k_drought_seconds = 600.0F;
  const float without_a_wave =
      snapshot.game_time - std::max(k_opening_grace_seconds, context.wave.ended_at);
  if (!context.wave.committed && without_a_wave > k_drought_seconds) {

    garrison_target =
        std::min(garrison_target, std::max(1, static_cast<int>(candidates.size()) / 3));
  }
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

    const std::unordered_set<Engine::Core::EntityID> marching(wave.members.begin(),
                                                              wave.members.end());
    const int wave_capacity = wave_capacity_for(context, required);
    for (const auto* entity : candidates) {

      if (static_cast<int>(wave.members.size()) >= wave_capacity) {
        break;
      }
      if (marching.contains(entity->id) || garrison.contains(entity->id)) {
        continue;
      }
      wave.members.push_back(entity->id);
      centre_x += entity->pos_x;
      centre_z += entity->pos_z;
    }

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

  if (static_cast<int>(available.size()) < required ||
      !commander_may_attack(context.strategy_config)) {
    wave.assembling = false;
    wave.assembled = 0;
    wave.assembly_required = 0;
    return;
  }

  if (!wave.assembling) {
    wave.assembling = true;
    wave.assembling_since = snapshot.game_time;
    wave.ready_since = -1000.0F;
  }
  constexpr float k_assembled_share = 0.60F;
  constexpr float k_assembly_patience_seconds = 30.0F;
  const float assembly_radius = std::max(8.0F, context.macro_targets.assembly_radius);
  int assembled = 0;
  for (const auto* entity : available) {
    if (distance_squared(entity->pos_x,
                         0.0F,
                         entity->pos_z,
                         context.station.x,
                         0.0F,
                         context.station.z) > assembly_radius * assembly_radius) {
      continue;
    }
    if (station_standing(*entity, context, snapshot.game_time) ==
        StationStanding::Ready) {
      ++assembled;
    }
  }
  wave.assembled = assembled;
  wave.assembly_required =
      static_cast<int>(std::ceil(k_assembled_share * static_cast<float>(required)));
  constexpr float k_settle_seconds = 2.0F;
  if (assembled >= wave.assembly_required) {
    if (wave.ready_since < 0.0F) {
      wave.ready_since = snapshot.game_time;
    }
  } else {
    wave.ready_since = -1000.0F;
  }

  const bool settled = wave.ready_since >= 0.0F &&
                       snapshot.game_time - wave.ready_since >= k_settle_seconds;
  const bool out_of_patience =
      snapshot.game_time - wave.assembling_since >= k_assembly_patience_seconds;
  if (!settled && !out_of_patience) {
    return;
  }
  wave.departed_under_strength = !settled;

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

  std::vector<const EntitySnapshot*> marching;
  marching.reserve(available.size());
  for (const auto* entity : available) {
    if (station_standing(*entity, context, snapshot.game_time) ==
        StationStanding::Ready) {
      marching.push_back(entity);
    }
  }
  if (!settled) {

    marching = available;
    std::stable_sort(marching.begin(),
                     marching.end(),
                     [&context](const EntitySnapshot* a, const EntitySnapshot* b) {
                       return distance_squared(a->pos_x,
                                               0.0F,
                                               a->pos_z,
                                               context.station.x,
                                               0.0F,
                                               context.station.z) <
                              distance_squared(b->pos_x,
                                               0.0F,
                                               b->pos_z,
                                               context.station.x,
                                               0.0F,
                                               context.station.z);
                     });
  }
  if (static_cast<int>(marching.size()) <
      minimum_deployable_strength(context, required)) {
    wave.assembling_since = snapshot.game_time;
    wave.departed_under_strength = false;
    return;
  }

  const int wave_capacity = wave_capacity_for(context, required);
  wave.members.clear();
  wave.members.reserve(marching.size());
  for (const auto* entity : marching) {
    if (static_cast<int>(wave.members.size()) >= wave_capacity) {
      break;
    }
    wave.members.push_back(entity->id);
  }
  wave.initial_size = static_cast<int>(wave.members.size());
  wave.target_id = target->id;
  wave.target_x = target->pos_x;
  wave.target_z = target->pos_z;
  wave.committed = true;
  wave.committed_at = snapshot.game_time;
  wave.assembling = false;
  wave.ready_since = -1000.0F;
}

auto wave_objective(const AISnapshot& snapshot,
                    const AIContext& context) -> const ContactSnapshot* {
  if (!context.wave.committed || context.wave.target_id == 0) {
    return nullptr;
  }
  return find_contact(snapshot, context.wave.target_id);
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
