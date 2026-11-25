#include "ai_attack_wave.h"

#include <QDebug>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "../../units/spawn_type.h"
#include "../../units/troop_type.h"
#include "ai_doctrine_catalog.h"
#include "ai_settlement_frame.h"
#include "ai_stall_recovery.h"
#include "ai_tribute.h"
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

constexpr float k_wave_refill_radius = 30.0F;
constexpr float k_wave_call_up_seconds = 60.0F;
constexpr float k_wave_progress_metres = 4.0F;
constexpr float k_wave_stall_seconds = 180.0F;

auto is_commander_contact(const ContactSnapshot& contact) -> bool {
  const auto troop = Game::Units::spawn_typeToTroopType(contact.spawn_type);
  return troop.has_value() && Game::Units::is_commander_troop(*troop);
}

auto matches_target(const ContactSnapshot& contact, DoctrineTarget target) -> bool {
  switch (target) {
  case DoctrineTarget::Army:
    return !contact.is_building && !is_commander_contact(contact) &&
           Game::Units::combat_role(contact.spawn_type) !=
               Game::Units::CombatRole::Noncombatant;
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

auto nearest_matching(const AISnapshot& snapshot,
                      const std::vector<ContactSnapshot>& contacts,
                      DoctrineTarget target_kind,
                      float from_x,
                      float from_z) -> const ContactSnapshot* {
  const ContactSnapshot* best = nullptr;
  float best_distance_sq = std::numeric_limits<float>::infinity();
  for (const auto& contact : contacts) {
    if (contact.health <= 0 || !matches_target(contact, target_kind) ||
        is_gold_vein_anchor(snapshot, contact.id)) {
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
    if (const auto* seen = nearest_matching(
            snapshot, snapshot.visible_enemies, target_kind, from_x, from_z)) {
      return seen;
    }
  }

  for (const auto target_kind : target_priority_for(context)) {
    if (const auto* known = nearest_matching(
            snapshot, snapshot.strategic_objectives, target_kind, from_x, from_z)) {
      return known;
    }
  }
  return nullptr;
}

auto scout_point(const AISnapshot& snapshot,
                 const AIContext& context,
                 float from_x,
                 float from_z,
                 float& out_x,
                 float& out_z) -> bool {
  if (!snapshot.has_map_bounds || !context.has_base_anchor) {
    return false;
  }
  constexpr float k_inset = 0.15F;
  constexpr float k_home_radius = 40.0F;
  const float inset_x = (snapshot.map_max_x - snapshot.map_min_x) * k_inset;
  const float inset_z = (snapshot.map_max_z - snapshot.map_min_z) * k_inset;
  const float min_x = snapshot.map_min_x + inset_x;
  const float max_x = snapshot.map_max_x - inset_x;
  const float min_z = snapshot.map_min_z + inset_z;
  const float max_z = snapshot.map_max_z - inset_z;
  if (distance_squared(
          from_x, 0.0F, from_z, context.base_pos_x, 0.0F, context.base_pos_z) <
      k_home_radius * k_home_radius) {
    out_x = std::clamp(
        snapshot.map_min_x + snapshot.map_max_x - context.base_pos_x, min_x, max_x);
    out_z = std::clamp(
        snapshot.map_min_z + snapshot.map_max_z - context.base_pos_z, min_z, max_z);
    return true;
  }
  const std::array<std::pair<float, float>, 4> corners{
      {{min_x, min_z}, {max_x, min_z}, {max_x, max_z}, {min_x, max_z}}};
  std::size_t nearest = 0;
  float best = std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < corners.size(); ++i) {
    const float dist_sq = distance_squared(
        corners[i].first, 0.0F, corners[i].second, from_x, 0.0F, from_z);
    if (dist_sq < best) {
      best = dist_sq;
      nearest = i;
    }
  }
  const auto& next = corners[(nearest + 1) % corners.size()];
  out_x = next.first;
  out_z = next.second;
  return true;
}

constexpr float k_scout_arrival_metres = 20.0F;
constexpr float k_scout_patience_seconds = 30.0F;

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

constexpr float k_sighting_memory_seconds = 120.0F;
constexpr float k_sighting_cleared_metres = 10.0F;

constexpr float k_defence_reach = 40.0F;
constexpr float k_reinforcement_reach = 80.0F;
constexpr float k_reinforcement_weight = 0.5F;
constexpr float k_tower_reach = 28.0F;
constexpr float k_tower_weight = 1.5F;
constexpr float k_commander_weight = 2.0F;
constexpr float k_commit_ratio = 1.25F;
constexpr float k_drought_commit_ratio = 1.0F;
constexpr float k_battle_reach = 26.0F;
constexpr float k_target_battle_metres = 40.0F;
constexpr float k_withdraw_ratio = 0.6F;
constexpr float k_bleeding_withdraw_ratio = 0.9F;
constexpr float k_bleeding_losses = 0.25F;
constexpr float k_withdraw_seconds = 30.0F;

auto health_share(int health, int max_health) -> float {
  if (max_health <= 0) {
    return health > 0 ? 1.0F : 0.0F;
  }
  return std::clamp(
      static_cast<float>(health) / static_cast<float>(max_health), 0.0F, 1.0F);
}

auto fights_in_the_field(const ContactSnapshot& contact) -> bool {
  return !contact.is_building && contact.health > 0 && is_war_contact(contact) &&
         Game::Units::combat_role(contact.spawn_type) !=
             Game::Units::CombatRole::Noncombatant;
}

auto field_weight(const ContactSnapshot& contact) -> float {
  return (is_commander_contact(contact) ? k_commander_weight : 1.0F) *
         health_share(contact.health, contact.max_health);
}

void note_enemy_sightings(const AISnapshot& snapshot, AIContext& context) {
  auto& sightings = context.enemy_sightings;
  std::unordered_set<Engine::Core::EntityID> in_view;
  for (const auto& contact : snapshot.visible_enemies) {
    if (!fights_in_the_field(contact)) {
      continue;
    }
    in_view.insert(contact.id);
    sightings[contact.id] = {.x = contact.pos_x,
                             .z = contact.pos_z,
                             .weight = field_weight(contact),
                             .seen_at = snapshot.game_time};
  }

  for (auto it = sightings.begin(); it != sightings.end();) {
    bool gone = snapshot.game_time - it->second.seen_at > k_sighting_memory_seconds;
    if (!gone && !in_view.contains(it->first)) {
      for (const auto& own : snapshot.friendly_units) {
        if (!own.is_building && own.health > 0 &&
            distance_squared(
                own.pos_x, 0.0F, own.pos_z, it->second.x, 0.0F, it->second.z) <
                k_sighting_cleared_metres * k_sighting_cleared_metres) {
          gone = true;
          break;
        }
      }
    }
    it = gone ? sightings.erase(it) : std::next(it);
  }
}

auto towers_covering(const AISnapshot& snapshot, float x, float z) -> float {
  float weight = 0.0F;
  for (const auto& known : snapshot.strategic_objectives) {
    if (known.health <= 0 || known.spawn_type != Game::Units::SpawnType::DefenseTower ||
        !is_war_contact(known)) {
      continue;
    }
    if (distance_squared(known.pos_x, 0.0F, known.pos_z, x, 0.0F, z) <=
        k_tower_reach * k_tower_reach) {
      weight += k_tower_weight * health_share(known.health, known.max_health);
    }
  }
  return weight;
}

auto defence_at(const AISnapshot& snapshot,
                const AIContext& context,
                float x,
                float z) -> float {
  float weight = towers_covering(snapshot, x, z);
  for (const auto& [id, seen] : context.enemy_sightings) {
    (void)id;
    const float distance_sq = distance_squared(seen.x, 0.0F, seen.z, x, 0.0F, z);
    if (distance_sq <= k_defence_reach * k_defence_reach) {
      weight += seen.weight;
    } else if (distance_sq <= k_reinforcement_reach * k_reinforcement_reach) {
      weight += k_reinforcement_weight * seen.weight;
    }
  }
  return weight;
}

auto strength_of(const std::vector<const EntitySnapshot*>& units) -> float {
  float strength = 0.0F;
  for (const auto* unit : units) {
    strength += health_share(unit->health, unit->max_health);
  }
  return strength;
}

auto select_beatable_target(const AISnapshot& snapshot,
                            const AIContext& context,
                            float from_x,
                            float from_z,
                            float strength,
                            float ratio,
                            bool& any_known,
                            float& defence_seen) -> const ContactSnapshot* {
  any_known = false;
  defence_seen = 0.0F;
  float weakest = std::numeric_limits<float>::infinity();
  for (const auto target_kind : target_priority_for(context)) {
    std::vector<std::pair<float, const ContactSnapshot*>> options;
    for (const auto* pool :
         {&snapshot.visible_enemies, &snapshot.strategic_objectives}) {
      for (const auto& contact : *pool) {
        if (contact.health <= 0 || !matches_target(contact, target_kind) ||
            is_gold_vein_anchor(snapshot, contact.id) || !is_war_contact(contact)) {
          continue;
        }
        options.emplace_back(
            distance_squared(contact.pos_x, 0.0F, contact.pos_z, from_x, 0.0F, from_z),
            &contact);
      }
    }
    std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) {
      return a.first < b.first;
    });
    for (const auto& [distance_sq, contact] : options) {
      (void)distance_sq;
      any_known = true;
      const float defence =
          defence_at(snapshot, context, contact->pos_x, contact->pos_z);
      weakest = std::min(weakest, defence);
      if (strength >= ratio * defence) {
        defence_seen = defence;
        return contact;
      }
    }
  }
  defence_seen = std::isfinite(weakest) ? weakest : 0.0F;
  return nullptr;
}

void withdraw_wave(AIContext& context, float game_time) {
  auto& wave = context.wave;
  wave.withdrawing = wave.members;
  wave.withdraw_until = game_time + k_withdraw_seconds;
  wave.committed = false;
  wave.members.clear();
  wave.target_id = 0;
  wave.best_gap = -1.0F;
  wave.ended_at = game_time;
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

namespace {

auto join_an_ally_attack(const AISnapshot& snapshot,
                         AIContext& context,
                         const std::vector<const EntitySnapshot*>& available,
                         int required) -> bool {
  constexpr float k_join_reach = 60.0F;
  if (snapshot.ally_attacks.empty() || !commander_may_attack(context.strategy_config)) {
    return false;
  }
  const int enough = std::max(3, (required + 1) / 2);
  if (static_cast<int>(available.size()) < enough) {
    return false;
  }
  const ContactSnapshot* best = nullptr;
  float best_sq = k_join_reach * k_join_reach;
  for (const auto& front : snapshot.ally_attacks) {
    for (const auto* pool :
         {&snapshot.visible_enemies, &snapshot.strategic_objectives}) {
      for (const auto& contact : *pool) {
        if (contact.health <= 0 || !is_war_contact(contact)) {
          continue;
        }
        const float sq = distance_squared(
            contact.pos_x, 0.0F, contact.pos_z, front.pos_x, 0.0F, front.pos_z);
        if (sq < best_sq) {
          best_sq = sq;
          best = &contact;
        }
      }
    }
  }
  if (best == nullptr) {
    return false;
  }
  auto& wave = context.wave;
  const int capacity = wave_capacity_for(context, required);
  wave.members.clear();
  for (const auto* entity : available) {
    if (static_cast<int>(wave.members.size()) >= capacity) {
      break;
    }
    wave.members.push_back(entity->id);
  }
  wave.initial_size = static_cast<int>(wave.members.size());
  wave.target_id = best->id;
  wave.target_x = best->pos_x;
  wave.target_z = best->pos_z;
  wave.committed = true;
  wave.committed_at = snapshot.game_time;
  wave.best_gap = -1.0F;
  wave.progress_at = snapshot.game_time;
  wave.assembling = false;
  wave.departed_under_strength = false;
  wave.ready_since = -1000.0F;
  return true;
}

auto called_attack(const AISnapshot& snapshot) -> const AllyPledge* {
  for (const auto& pledge : snapshot.pledges) {
    if (pledge.kind == Game::Systems::AllyCallKind::Attack &&
        pledge.expires_at >= snapshot.game_time) {
      return &pledge;
    }
  }
  return nullptr;
}

auto answer_an_ally_call(const AISnapshot& snapshot,
                         AIContext& context,
                         const std::vector<const EntitySnapshot*>& available,
                         int required) -> bool {
  constexpr float k_rest_after_wave = 15.0F;
  const auto* pledge = called_attack(snapshot);
  if (pledge == nullptr ||
      static_cast<int>(available.size()) < k_ally_call_min_attackers ||
      snapshot.game_time - context.wave.ended_at < k_rest_after_wave) {
    return false;
  }
  const ContactSnapshot* target = find_contact(snapshot, pledge->target);
  if (target == nullptr) {
    return false;
  }
  auto& wave = context.wave;
  const int capacity = wave_capacity_for(context, required);
  wave.members.clear();
  for (const auto* entity : available) {
    if (static_cast<int>(wave.members.size()) >= capacity) {
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
  wave.best_gap = -1.0F;
  wave.progress_at = snapshot.game_time;
  wave.assembling = false;
  wave.departed_under_strength = false;
  wave.ready_since = -1000.0F;
  return true;
}

} // namespace

void update_attack_wave(const AISnapshot& snapshot, AIContext& context) {
  auto& wave = context.wave;
  note_enemy_sightings(snapshot, context);
  if (snapshot.game_time > wave.withdraw_until) {
    wave.withdrawing.clear();
  } else {
    constexpr float k_home_metres = 14.0F;
    std::erase_if(wave.withdrawing, [&](Engine::Core::EntityID id) {
      const auto* entity = find_friendly(snapshot, id);
      return entity == nullptr || entity->health <= 0 ||
             (context.has_base_anchor &&
              distance_squared(entity->pos_x,
                               0.0F,
                               entity->pos_z,
                               context.base_pos_x,
                               0.0F,
                               context.base_pos_z) <= k_home_metres * k_home_metres);
    });
  }
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
    bool engaged = false;
    for (const auto id : wave.members) {
      const auto* entity = find_friendly(snapshot, id);
      if (entity == nullptr || entity->health <= 0) {
        continue;
      }
      survivors.push_back(id);
      centre_x += entity->pos_x;
      centre_z += entity->pos_z;
      engaged = engaged || entity->engaged;
    }
    wave.members = std::move(survivors);

    if (!wave.members.empty()) {
      const float count = static_cast<float>(wave.members.size());
      const float front_x = centre_x / count;
      const float front_z = centre_z / count;
      const std::unordered_set<Engine::Core::EntityID> marching(wave.members.begin(),
                                                                wave.members.end());
      const bool calls_up_home =
          context.strategy_config.doctrine == nullptr &&
          snapshot.game_time - wave.committed_at > k_wave_call_up_seconds;
      const int wave_capacity = calls_up_home ? static_cast<int>(candidates.size())
                                              : wave_capacity_for(context, required);
      for (const auto* entity : candidates) {
        if (static_cast<int>(wave.members.size()) >= wave_capacity) {
          break;
        }
        if (marching.contains(entity->id) || garrison.contains(entity->id)) {
          continue;
        }
        if (!calls_up_home &&
            distance_squared(
                entity->pos_x, 0.0F, entity->pos_z, front_x, 0.0F, front_z) >
                k_wave_refill_radius * k_wave_refill_radius) {
          continue;
        }
        wave.members.push_back(entity->id);
        centre_x += entity->pos_x;
        centre_z += entity->pos_z;
      }
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

    const float gap = std::sqrt(
        distance_squared(centre_x, 0.0F, centre_z, wave.target_x, 0.0F, wave.target_z));
    if (engaged || wave.best_gap < 0.0F ||
        gap < wave.best_gap - k_wave_progress_metres) {
      wave.best_gap = wave.best_gap < 0.0F ? gap : std::min(wave.best_gap, gap);
      wave.progress_at = snapshot.game_time;
    }
    if (snapshot.game_time - std::max(wave.progress_at, wave.committed_at) >
        k_wave_stall_seconds) {
      wave.committed = false;
      wave.members.clear();
      wave.target_id = 0;
      wave.best_gap = -1.0F;
      wave.ended_at = snapshot.game_time;
      return;
    }

    std::vector<const EntitySnapshot*> marching_now;
    marching_now.reserve(wave.members.size());
    for (const auto id : wave.members) {
      if (const auto* entity = find_friendly(snapshot, id)) {
        marching_now.push_back(entity);
      }
    }
    const float ours = strength_of(marching_now);
    {
      float theirs = towers_covering(snapshot, centre_x, centre_z);
      for (const auto& contact : snapshot.visible_enemies) {
        if (fights_in_the_field(contact) &&
            distance_squared(
                contact.pos_x, 0.0F, contact.pos_z, centre_x, 0.0F, centre_z) <=
                k_battle_reach * k_battle_reach) {
          theirs += field_weight(contact);
        }
      }
      theirs = std::max(theirs, defence_at(snapshot, context, centre_x, centre_z));
      if (wave.target_id != 0 && gap <= k_target_battle_metres) {
        theirs = std::max(theirs,
                          defence_at(snapshot, context, wave.target_x, wave.target_z));
      }
      const float losses =
          1.0F - static_cast<float>(remaining) /
                     static_cast<float>(std::max(1, wave.initial_size));
      wave.last_strength = ours;
      wave.last_defence = theirs;

      const bool at_their_works =
          towers_covering(snapshot, centre_x, centre_z) > 0.0F ||
          (wave.target_id != 0 && gap <= k_target_battle_metres && [&] {
            const auto* objective = find_contact(snapshot, wave.target_id);
            return objective != nullptr && objective->is_building;
          }());
      if (at_their_works && theirs > 0.0F &&
          (ours < k_withdraw_ratio * theirs ||
           (losses >= k_bleeding_losses &&
            ours < k_bleeding_withdraw_ratio * theirs))) {
        withdraw_wave(context, snapshot.game_time);
        return;
      }
    }

    if (const auto* pledge = called_attack(snapshot);
        pledge != nullptr && pledge->target != wave.target_id &&
        find_contact(snapshot, pledge->target) != nullptr) {
      wave.target_id = pledge->target;
      wave.best_gap = -1.0F;
      wave.progress_at = snapshot.game_time;
    }
    const ContactSnapshot* target = find_contact(snapshot, wave.target_id);
    if (target == nullptr) {
      target = select_wave_target(snapshot, context, centre_x, centre_z);
      if (target == nullptr && wave.target_id == 0) {
        const bool arrived =
            gap <= k_scout_arrival_metres ||
            snapshot.game_time - wave.progress_at > k_scout_patience_seconds;
        if (arrived &&
            scout_point(
                snapshot, context, centre_x, centre_z, wave.target_x, wave.target_z)) {
          wave.best_gap = -1.0F;
          wave.progress_at = snapshot.game_time;
        }
        return;
      }
      if (target == nullptr) {
        wave.committed = false;
        wave.members.clear();
        wave.target_id = 0;
        wave.ended_at = snapshot.game_time;
        return;
      }

      if (ours < k_commit_ratio *
                     defence_at(snapshot, context, target->pos_x, target->pos_z)) {
        bool any_known = false;
        float defence = 0.0F;
        target = select_beatable_target(snapshot,
                                        context,
                                        centre_x,
                                        centre_z,
                                        ours,
                                        k_commit_ratio,
                                        any_known,
                                        defence);
        if (target == nullptr) {
          wave.last_defence = defence;
          withdraw_wave(context, snapshot.game_time);
          return;
        }
      }
      wave.target_id = target->id;
    }
    wave.target_x = target->pos_x;
    wave.target_z = target->pos_z;
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

  if (answer_an_ally_call(snapshot, context, available, required)) {
    return;
  }

  if (snapshot.game_time - wave.ended_at < regroup_seconds_for(context)) {
    return;
  }

  if (join_an_ally_attack(snapshot, context, available, required)) {
    return;
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

  constexpr float k_levy_wait_seconds = 20.0F;
  if (snapshot.seconds_to_next_levy >= 0.0F &&
      snapshot.seconds_to_next_levy < k_levy_wait_seconds &&
      !context.barracks_under_threat) {
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

  const int column = std::min(static_cast<int>(available.size()),
                              wave_capacity_for(context, required));
  std::vector<const EntitySnapshot*> column_units(
      available.begin(), available.begin() + std::max(0, column));
  const float strength = strength_of(column_units);
  const float commit_ratio =
      without_a_wave > k_drought_seconds ? k_drought_commit_ratio : k_commit_ratio;
  bool any_known = false;
  float defence = 0.0F;
  const ContactSnapshot* target = select_beatable_target(snapshot,
                                                         context,
                                                         centre_x,
                                                         centre_z,
                                                         strength,
                                                         commit_ratio,
                                                         any_known,
                                                         defence);
  wave.last_strength = strength;
  wave.last_defence = defence;
  if (target == nullptr && any_known) {
    wave.declined = 1;
    wave.assembling_since = snapshot.game_time;
    wave.ready_since = -1000.0F;
    wave.departed_under_strength = false;
    return;
  }
  wave.declined = 0;
  float scout_x = 0.0F;
  float scout_z = 0.0F;
  if (target == nullptr &&
      !scout_point(snapshot, context, centre_x, centre_z, scout_x, scout_z)) {
    wave.assembling = false;
    wave.ready_since = -1000.0F;
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
  wave.target_id = target != nullptr ? target->id : 0;
  wave.target_x = target != nullptr ? target->pos_x : scout_x;
  wave.target_z = target != nullptr ? target->pos_z : scout_z;
  wave.committed = true;
  wave.committed_at = snapshot.game_time;
  wave.best_gap = -1.0F;
  wave.progress_at = snapshot.game_time;
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
