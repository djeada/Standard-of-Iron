#include "commander_duel.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

#include "../../audio/cue_ids.h"
#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../duel_spacing.h"
#include "../navigation/pathfinding.h"
#include "../navigation/walkability.h"
#include "animation/showcase_pose_manifest.h"
#include "attack_control.h"
#include "combat_hit_resolver.h"
#include "combat_random.h"
#include "combat_utils.h"
#include "damage_application.h"
#include "target_assignment.h"

namespace Game::Systems::Combat {

namespace {

using Engine::Core::CommanderDuelComponent;
using Game::Systems::CombatActions::CombatActionId;
using Game::Systems::CombatActions::WeaponFamily;
using Outcome = Engine::Core::CommanderDuelOutcome;
using Phase = Engine::Core::CommanderDuelPhase;

constexpr float k_engage_distance = 8.0F;
constexpr float k_lunge_distance = 3.3F;
constexpr float k_lunge_slack = 0.35F;
constexpr float k_standoff_distance = 4.8F;
constexpr float k_crowd_radius = 3.2F;
constexpr float k_dash_speed = 7.5F;
constexpr float k_press_speed = 3.2F;
constexpr float k_circle_speed = 3.0F;
constexpr float k_circle_degrees_per_second = 34.0F;
constexpr float k_execution_health_fraction = 0.15F;
constexpr float k_evade_seconds = 0.85F;
constexpr float k_evade_reads_swing_at = 0.20F;

enum class LinkMove : std::uint8_t {
  Lunge,
  LightA,
  LightB,
  Thrust,
  Spin,
  Launcher,
  AirA,
  AirB,
  Dive,
  Heavy,
  Finisher,
};

struct Link {
  LinkMove move;
  Outcome outcome;
};

constexpr std::array<Link, 5> k_string_press{{
    {LinkMove::Lunge, Outcome::Clash},
    {LinkMove::LightA, Outcome::Parry},
    {LinkMove::LightB, Outcome::Parry},
    {LinkMove::Spin, Outcome::Hit},
    {LinkMove::Finisher, Outcome::Finish},
}};

constexpr std::array<Link, 4> k_string_slip{{
    {LinkMove::Lunge, Outcome::Hit},
    {LinkMove::LightA, Outcome::Parry},
    {LinkMove::LightB, Outcome::Hit},
    {LinkMove::Thrust, Outcome::Evade},
}};

constexpr std::array<Link, 6> k_string_juggle{{
    {LinkMove::Lunge, Outcome::Parry},
    {LinkMove::LightA, Outcome::Hit},
    {LinkMove::Launcher, Outcome::Launch},
    {LinkMove::AirA, Outcome::Juggle},
    {LinkMove::AirB, Outcome::Juggle},
    {LinkMove::Dive, Outcome::Finish},
}};

constexpr std::array<Link, 6> k_string_bind{{
    {LinkMove::Lunge, Outcome::Clash},
    {LinkMove::Heavy, Outcome::Clash},
    {LinkMove::LightA, Outcome::Hit},
    {LinkMove::LightB, Outcome::Parry},
    {LinkMove::Spin, Outcome::Hit},
    {LinkMove::Finisher, Outcome::Finish},
}};

constexpr std::array<std::span<const Link>, 4> k_strings{{
    k_string_press,
    k_string_slip,
    k_string_juggle,
    k_string_bind,
}};

[[nodiscard]] auto outcome_damage_multiplier(Outcome outcome) -> float {
  switch (outcome) {
  case Outcome::Parry:
    return 0.5F;
  case Outcome::Hit:
  case Outcome::Launch:
    return 2.0F;
  case Outcome::Juggle:
    return 1.5F;
  case Outcome::Finish:
    return 4.0F;
  case Outcome::None:
  case Outcome::Evade:
  case Outcome::Clash:
  case Outcome::ClashReply:
    break;
  }
  return 0.0F;
}

[[nodiscard]] auto
weapon_family_of(const Engine::Core::Entity& entity) -> WeaponFamily {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return WeaponFamily::None;
  }
  switch (Engine::Core::resolve_combat_attack_family(
      unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Melee)) {
  case Engine::Core::CombatAttackFamily::Sword:
    return WeaponFamily::Sword;
  case Engine::Core::CombatAttackFamily::Spear:
    return WeaponFamily::Spear;
  case Engine::Core::CombatAttackFamily::Bow:
  case Engine::Core::CombatAttackFamily::None:
    break;
  }
  return WeaponFamily::None;
}

[[nodiscard]] auto action_for(LinkMove move, WeaponFamily weapon) -> CombatActionId {
  bool const spear = weapon == WeaponFamily::Spear;
  switch (move) {
  case LinkMove::Lunge:
    return spear ? CombatActionId::CommanderSpearGapCloser
                 : CombatActionId::CommanderSwordGapCloser;
  case LinkMove::LightA:
    return spear ? CombatActionId::RpgSpearThrust : CombatActionId::RpgSwordSlashLeft;
  case LinkMove::LightB:
    return spear ? CombatActionId::CommanderSpearStepThrust
                 : CombatActionId::RpgSwordSlashRight;
  case LinkMove::Thrust:
    return spear ? CombatActionId::RpgSpearThrust : CombatActionId::RpgSwordThrust;
  case LinkMove::Spin:
    return spear ? CombatActionId::RpgSpearSweep : CombatActionId::CommanderSwordSpin;
  case LinkMove::Launcher:
    return spear ? CombatActionId::CommanderSpearLauncher
                 : CombatActionId::CommanderSwordLauncher;
  case LinkMove::AirA:
    return spear ? CombatActionId::CommanderSpearAirThrust
                 : CombatActionId::CommanderSwordAirLight;
  case LinkMove::AirB:
    return spear ? CombatActionId::CommanderSpearAirThrust
                 : CombatActionId::CommanderSwordAirReverse;
  case LinkMove::Dive:
    return spear ? CombatActionId::CommanderSpearDive
                 : CombatActionId::CommanderSwordDive;
  case LinkMove::Heavy:
    return spear ? CombatActionId::RpgSpearSweep : CombatActionId::RpgSwordOverhead;
  case LinkMove::Finisher:
    return spear ? CombatActionId::RpgSpearFinisher : CombatActionId::RpgSwordFinisher;
  }
  return CombatActionId::None;
}

[[nodiscard]] auto planar_distance(const Engine::Core::Entity& a,
                                   const Engine::Core::Entity& b) -> float {
  auto const* ta = a.get_component<Engine::Core::TransformComponent>();
  auto const* tb = b.get_component<Engine::Core::TransformComponent>();
  if (ta == nullptr || tb == nullptr) {
    return 0.0F;
  }
  return std::hypot(tb->position.x - ta->position.x, tb->position.z - ta->position.z);
}

[[nodiscard]] auto can_duel(const Engine::Core::Entity& entity) -> bool {
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (commander == nullptr || unit == nullptr || attack == nullptr ||
      commander->fpv_controlled || !commander->advanced_combat_enabled ||
      commander->wounded || unit->health <= 0 || !attack->attacks_in_melee() ||
      entity.has_component<Engine::Core::PendingRemovalComponent>() ||
      entity.get_component<Engine::Core::TransformComponent>() == nullptr) {
    return false;
  }
  return Game::Systems::DuelSpacing::is_duel_body(entity) &&
         weapon_family_of(entity) != WeaponFamily::None;
}

[[nodiscard]] auto
wanted_target(const Engine::Core::Entity& entity) -> Engine::Core::EntityID {
  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (attack != nullptr && attack->in_melee_lock && attack->melee_lock_target_id != 0) {
    return attack->melee_lock_target_id;
  }
  auto const* order = entity.get_component<Engine::Core::AttackTargetComponent>();
  return order != nullptr ? order->target_id : 0;
}

[[nodiscard]] auto pressed_by_others(Engine::Core::World& world,
                                     const CombatQueryContext& query_context,
                                     const Engine::Core::Entity& self,
                                     const Engine::Core::Entity& opponent) -> bool {
  auto const* unit = self.get_component<Engine::Core::UnitComponent>();
  auto const* transform = self.get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr) {
    return true;
  }
  collect_unit_ids_near(world,
                        transform->position.x,
                        transform->position.z,
                        k_crowd_radius,
                        query_context.nearby_unit_ids);
  for (auto const other_id : query_context.nearby_unit_ids) {
    if (other_id == self.get_id() || other_id == opponent.get_id()) {
      continue;
    }
    auto const* other = query_context.find_entity(other_id);
    auto const* other_unit = other != nullptr
                                 ? other->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
    auto const* other_transform =
        other != nullptr ? other->get_component<Engine::Core::TransformComponent>()
                         : nullptr;
    if (other_unit == nullptr || other_transform == nullptr ||
        other_unit->health <= 0 ||
        !query_context.hostile(unit->owner_id, other_unit->owner_id)) {
      continue;
    }
    if (std::hypot(other_transform->position.x - transform->position.x,
                   other_transform->position.z - transform->position.z) <=
        k_crowd_radius) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] auto pair_seed(Engine::Core::EntityID a,
                             Engine::Core::EntityID b) -> std::uint32_t {
  auto const lo = static_cast<std::uint32_t>(std::min(a, b));
  auto const hi = static_cast<std::uint32_t>(std::max(a, b));
  return mix_hash32((lo * 2246822519U) ^ (hi * 3266489917U) ^ 0x51ED270BU);
}

[[nodiscard]] auto lower_id_leads_round(Engine::Core::EntityID a,
                                        Engine::Core::EntityID b,
                                        std::uint8_t round) -> bool {
  bool const flipped = (std::popcount(static_cast<unsigned>(round)) & 1) != 0;
  bool const seed_bit = (pair_seed(a, b) & 1U) != 0U;
  return flipped != seed_bit;
}

struct Pair {
  Engine::Core::Entity* first{nullptr};
  Engine::Core::Entity* second{nullptr};
  CommanderDuelComponent* first_duel{nullptr};
  CommanderDuelComponent* second_duel{nullptr};

  [[nodiscard]] auto aggressor() const -> Engine::Core::Entity* {
    return first_duel->aggressor ? first : second;
  }
  [[nodiscard]] auto defender() const -> Engine::Core::Entity* {
    return first_duel->aggressor ? second : first;
  }
  [[nodiscard]] auto aggressor_duel() const -> CommanderDuelComponent* {
    return first_duel->aggressor ? first_duel : second_duel;
  }
  [[nodiscard]] auto defender_duel() const -> CommanderDuelComponent* {
    return first_duel->aggressor ? second_duel : first_duel;
  }
};

void set_motion(CommanderDuelComponent& duel,
                float separation,
                float speed,
                float orbit_degrees_per_second) {
  duel.desired_separation = separation;
  duel.approach_speed = speed;
  duel.orbit_degrees_per_second = orbit_degrees_per_second;
}

void enter_phase(const Pair& pair, Phase phase) {
  for (auto* duel : {pair.first_duel, pair.second_duel}) {
    duel->phase = phase;
    duel->phase_time = 0.0F;
    duel->attack_permitted = false;
    duel->clash_reply_requested = false;
    duel->evade_started = false;
    duel->pending_outcome = Outcome::None;
    duel->swing_delay = 0.0F;
  }
  float const contact =
      Game::Systems::DuelSpacing::standoff_between(*pair.first, *pair.second).preferred;
  std::uint32_t const seed = pair_seed(pair.first->get_id(), pair.second->get_id());
  float const turn = ((seed >> 1U) & 1U) != 0U ? 1.0F : -1.0F;
  auto* aggressor = pair.aggressor_duel();
  auto* defender = pair.defender_duel();
  switch (phase) {
  case Phase::Closing:
    set_motion(*aggressor, k_lunge_distance, k_dash_speed, 0.0F);
    set_motion(*defender,
               k_lunge_distance,
               aggressor->round == 0U ? k_dash_speed : 1.4F,
               0.0F);
    break;
  case Phase::Exchange:
    aggressor->link = 0U;
    aggressor->round_decided = false;
    aggressor->attack_permitted = true;
    defender->round_decided = false;
    set_motion(*aggressor, contact, k_press_speed, 0.0F);
    set_motion(*defender, contact, 0.0F, 0.0F);
    break;
  case Phase::Break:
    set_motion(*aggressor, contact, 0.0F, 0.0F);
    set_motion(*defender, contact, 0.0F, 0.0F);
    break;
  case Phase::Standoff: {
    float const length =
        1.0F + 0.9F * deterministic_unit_roll(seed, 17U + aggressor->round);
    for (auto* duel : {pair.first_duel, pair.second_duel}) {
      duel->phase_length = length;
      set_motion(*duel,
                 k_standoff_distance,
                 k_circle_speed,
                 turn * k_circle_degrees_per_second);
    }
    break;
  }
  }
}

void begin_round(const Pair& pair, std::uint8_t round) {
  bool const first_leads =
      lower_id_leads_round(pair.first->get_id(), pair.second->get_id(), round);
  pair.first_duel->aggressor = first_leads;
  pair.second_duel->aggressor = !first_leads;
  pair.first_duel->round = round;
  pair.second_duel->round = round;
  enter_phase(pair, Phase::Closing);
}

void play_cue(const char* cue) {
  Engine::Core::EventManager::instance().publish(Engine::Core::AudioCueEvent(cue));
}

void start_evade(Engine::Core::Entity& defender) {
  play_cue(Game::Audio::Cue::k_combat_dodge);
  auto* routine =
      Engine::Core::get_or_add_component<Engine::Core::ShowcaseRoutineComponent>(
          &defender);
  if (routine == nullptr) {
    return;
  }
  *routine = Engine::Core::ShowcaseRoutineComponent{};
  routine->loop = false;
  routine->steps.push_back(
      {.move = static_cast<std::uint8_t>(Animation::HumanoidShowcaseMove::SideAerial),
       .duration = k_evade_seconds,
       .hold_after = 0.0F});
}

[[nodiscard]] auto has_room_to_evade(const Engine::Core::Entity& defender) -> bool {
  auto const* transform = defender.get_component<Engine::Core::TransformComponent>();
  auto const* movement = defender.get_component<Engine::Core::MovementComponent>();
  if (transform == nullptr) {
    return false;
  }
  auto const travel = Animation::humanoid_showcase_root_travel(
      Animation::HumanoidShowcaseMove::SideAerial, 1.0F);
  float const scale = std::max(0.01F, transform->scale.x);
  float const yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const dx = travel.x * scale;
  float const dz = travel.z * scale;
  QVector3D const from(transform->position.x, 0.0F, transform->position.z);
  QVector3D const landing(from.x() + (dx * std::cos(yaw)) + (dz * std::sin(yaw)),
                          0.0F,
                          from.z() + (dz * std::cos(yaw)) - (dx * std::sin(yaw)));
  Game::Systems::BodyProfile profile;
  profile.radius = movement != nullptr ? movement->get_navigation_clearance() : 0.45F;
  profile.passability = movement != nullptr && movement->get_can_enter_forest()
                            ? Game::Systems::Pathfinding::Passability::Light
                            : Game::Systems::Pathfinding::Passability::Heavy;
  return Game::Systems::Walkability::can_stand(landing, profile) &&
         !structure_separates_positions(from, landing);
}

[[nodiscard]] auto is_flipping(const Engine::Core::Entity& entity) -> bool {
  auto const* routine = entity.get_component<Engine::Core::ShowcaseRoutineComponent>();
  return routine != nullptr && !routine->finished && !routine->steps.empty();
}

[[nodiscard]] auto has_settled(const Engine::Core::Entity& entity,
                               const CommanderDuelComponent& duel) -> bool {
  constexpr float k_settled_slide_speed = 0.6F;
  return duel.tumble_remaining <= 0.0F &&
         std::hypot(duel.slide_vx, duel.slide_vz) < k_settled_slide_speed &&
         !entity.has_component<Engine::Core::StaggerComponent>() &&
         !entity.has_component<Engine::Core::CombatLaunchComponent>() &&
         !is_flipping(entity);
}

[[nodiscard]] auto is_swinging(const Engine::Core::Entity& entity) -> bool {
  auto const* action =
      entity.get_component<Engine::Core::RpgCommanderActionComponent>();
  return action != nullptr && action->action_running && action->combat_action_id != 0U;
}

void lock_on(Engine::Core::Entity& entity, const Engine::Core::Entity& opponent) {
  auto* attack = entity.get_component<Engine::Core::AttackComponent>();
  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (attack == nullptr || transform == nullptr) {
    return;
  }
  if (!attack->melee_locked_on(opponent.get_id())) {
    attack->in_melee_lock = true;
    attack->melee_lock_target_id = opponent.get_id();
    attack->melee_footwork_offset = 0.0F;
  }
  attack->melee_lock_separation_time = 0.0F;
  assign_attack_target(&entity, opponent.get_id(), TargetSource::MeleeLock);
  stop_unit_movement(&entity, transform);
}

void try_form_duel(Engine::Core::World& world,
                   const CombatQueryContext& query_context,
                   Engine::Core::Entity& entity) {
  if (!can_duel(entity)) {
    return;
  }
  auto* opponent = world.get_entity(wanted_target(entity));
  if (opponent == nullptr || opponent == &entity || !can_duel(*opponent) ||
      opponent->has_component<CommanderDuelComponent>() ||
      wanted_target(*opponent) != entity.get_id() ||
      planar_distance(entity, *opponent) > k_engage_distance ||
      pressed_by_others(world, query_context, entity, *opponent) ||
      pressed_by_others(world, query_context, *opponent, entity)) {
    return;
  }
  lock_on(entity, *opponent);
  lock_on(*opponent, entity);
  Pair pair{.first = &entity, .second = opponent};
  pair.first_duel = entity.add_component<CommanderDuelComponent>();
  pair.second_duel = opponent->add_component<CommanderDuelComponent>();
  if (pair.first_duel == nullptr || pair.second_duel == nullptr) {
    return;
  }
  pair.first_duel->opponent_id = opponent->get_id();
  pair.second_duel->opponent_id = entity.get_id();
  begin_round(pair, 0U);
}

[[nodiscard]] auto pair_still_stands(const Engine::Core::Entity& entity,
                                     const Engine::Core::Entity* opponent) -> bool {
  if (opponent == nullptr || !can_duel(entity) || !can_duel(*opponent)) {
    return false;
  }
  auto const* theirs = opponent->get_component<CommanderDuelComponent>();
  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  auto const* their_attack = opponent->get_component<Engine::Core::AttackComponent>();
  return theirs != nullptr && theirs->opponent_id == entity.get_id() &&
         attack->melee_locked_on(opponent->get_id()) &&
         their_attack->melee_locked_on(entity.get_id());
}

void advance_tumble(Engine::Core::Entity& entity,
                    CommanderDuelComponent& duel,
                    float delta_time);

[[nodiscard]] auto advance_pair(Engine::Core::World& world,
                                const CombatQueryContext& query_context,
                                const Pair& pair,
                                float delta_time) -> bool {
  for (auto* duel : {pair.first_duel, pair.second_duel}) {
    duel->phase_time += delta_time;
    duel->swing_delay = std::max(0.0F, duel->swing_delay - delta_time);
  }
  for (auto* entity : {pair.first, pair.second}) {
    if (auto* attack = entity->get_component<Engine::Core::AttackComponent>()) {
      attack->melee_lock_separation_time = 0.0F;
    }
  }
  advance_tumble(*pair.first, *pair.first_duel, delta_time);
  advance_tumble(*pair.second, *pair.second_duel, delta_time);
  auto* aggressor = pair.aggressor_duel();
  auto* defender = pair.defender_duel();
  float const distance = planar_distance(*pair.first, *pair.second);
  switch (aggressor->phase) {
  case Phase::Closing: {
    constexpr float k_closing_timeout = 3.0F;
    if (distance <= k_lunge_distance + k_lunge_slack ||
        aggressor->phase_time > k_closing_timeout) {
      enter_phase(pair, Phase::Exchange);
    }
    break;
  }
  case Phase::Exchange: {
    constexpr float k_exchange_timeout = 12.0F;
    bool const swinging = is_swinging(*pair.aggressor());
    if (aggressor->pending_outcome == Outcome::Evade && !defender->evade_started &&
        swinging) {
      auto const* action =
          pair.aggressor()->get_component<Engine::Core::RpgCommanderActionComponent>();
      if (action->normalized_action_time >= k_evade_reads_swing_at) {
        start_evade(*pair.defender());
        defender->evade_started = true;
      }
    }
    if ((aggressor->round_decided && !swinging) ||
        aggressor->phase_time > k_exchange_timeout) {
      enter_phase(pair, Phase::Break);
    }
    break;
  }
  case Phase::Break: {
    constexpr float k_break_floor = 0.45F;
    constexpr float k_break_timeout = 4.0F;
    bool const settled = has_settled(*pair.first, *pair.first_duel) &&
                         has_settled(*pair.second, *pair.second_duel);
    if ((settled && aggressor->phase_time >= k_break_floor) ||
        aggressor->phase_time > k_break_timeout) {
      if (pressed_by_others(world, query_context, *pair.first, *pair.second) ||
          pressed_by_others(world, query_context, *pair.second, *pair.first)) {
        return false;
      }
      enter_phase(pair, Phase::Standoff);
    }
    break;
  }
  case Phase::Standoff:
    if (aggressor->phase_time >= aggressor->phase_length) {
      begin_round(pair, static_cast<std::uint8_t>((aggressor->round + 1U) % 240U));
    }
    break;
  }
  return true;
}

void advance_tumble(Engine::Core::Entity& entity,
                    CommanderDuelComponent& duel,
                    float delta_time) {
  auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || duel.tumble_remaining <= 0.0F) {
    return;
  }
  duel.tumble_remaining = std::max(0.0F, duel.tumble_remaining - delta_time);
  commander->dodge_active = duel.tumble_remaining > 0.0F;
  commander->dodge_phase =
      1.0F - (duel.tumble_remaining / CommanderDuelComponent::k_tumble_seconds);
}

void end_tumble(Engine::Core::Entity& entity) {
  auto* duel = entity.get_component<CommanderDuelComponent>();
  auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (duel != nullptr && commander != nullptr && duel->tumble_remaining > 0.0F) {
    commander->dodge_active = false;
    commander->dodge_phase = 0.0F;
  }
}

void slide(CommanderDuelComponent& duel, float dir_x, float dir_z, float speed) {
  duel.slide_vx = dir_x * speed;
  duel.slide_vz = dir_z * speed;
}

void launch(Engine::Core::Entity& target,
            const Engine::Core::TransformComponent& target_transform,
            float dir_x,
            float dir_z,
            float vertical_speed) {
  auto* airborne = target.get_component<Engine::Core::CombatLaunchComponent>();
  if (airborne == nullptr) {
    airborne = target.add_component<Engine::Core::CombatLaunchComponent>();
    if (airborne == nullptr) {
      return;
    }
    airborne->ground_y = target_transform.position.y;
  }
  constexpr float k_carry_speed = 0.9F;
  airborne->velocity_y = vertical_speed;
  airborne->velocity_x = dir_x * k_carry_speed;
  airborne->velocity_z = dir_z * k_carry_speed;
}

void rise_for_air_link(Engine::Core::Entity& attacker, LinkMove move) {
  auto* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr) {
    return;
  }
  if (move == LinkMove::AirA) {
    play_cue(Game::Audio::Cue::k_combat_jump);
    commander->jump_active = true;
    commander->jump_phase = 0.0F;
    commander->airborne_velocity = 7.0F;
  } else if (move == LinkMove::AirB) {
    commander->jump_active = true;
    commander->airborne_velocity = std::max(commander->airborne_velocity, 3.2F);
  } else if (move == LinkMove::Dive) {
    commander->jump_active = true;
  }
}

} // namespace

void process_commander_duels(Engine::Core::World* world,
                             const CombatQueryContext& query_context,
                             float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }
  std::vector<Engine::Core::Entity*> commanders;
  for (auto [entity, commander] :
       world->entity_view<Engine::Core::CommanderComponent>()) {
    (void)commander;
    commanders.push_back(&entity);
  }
  std::sort(commanders.begin(), commanders.end(), [](auto const* a, auto const* b) {
    return a->get_id() < b->get_id();
  });

  std::vector<Engine::Core::EntityID> dissolved;
  for (auto* entity : commanders) {
    auto* duel = entity->get_component<CommanderDuelComponent>();
    if (duel == nullptr) {
      try_form_duel(*world, query_context, *entity);
      continue;
    }
    auto* opponent = world->get_entity(duel->opponent_id);
    if (!pair_still_stands(*entity, opponent)) {
      dissolved.push_back(entity->get_id());
      continue;
    }
    if (entity->get_id() > opponent->get_id()) {
      continue;
    }
    Pair const pair{.first = entity,
                    .second = opponent,
                    .first_duel = duel,
                    .second_duel = opponent->get_component<CommanderDuelComponent>()};
    if (!advance_pair(*world, query_context, pair, delta_time)) {
      dissolved.push_back(entity->get_id());
      dissolved.push_back(opponent->get_id());
    }
  }
  for (auto const id : dissolved) {
    if (auto* entity = world->get_entity(id)) {
      end_tumble(*entity);
    }
    world->remove<CommanderDuelComponent>(id);
  }
}

auto duelling_with(const Engine::Core::Entity& entity,
                   const Engine::Core::Entity& other) -> bool {
  auto const* duel = entity.get_component<CommanderDuelComponent>();
  return duel != nullptr && duel->opponent_id == other.get_id();
}

auto duel_forces_swing(const Engine::Core::Entity& entity) -> bool {
  auto const* duel = entity.get_component<CommanderDuelComponent>();
  if (duel == nullptr) {
    return false;
  }
  return duel->clash_reply_requested ||
         (duel->aggressor && duel->phase == Phase::Exchange && duel->attack_permitted &&
          !duel->round_decided && duel->swing_delay <= 0.0F);
}

auto duel_permits_attack(const Engine::Core::Entity& entity) -> bool {
  return !entity.has_component<CommanderDuelComponent>() || duel_forces_swing(entity);
}

auto duel_permits_signature(const Engine::Core::Entity& entity) -> bool {
  auto const* duel = entity.get_component<CommanderDuelComponent>();
  return duel == nullptr || duel->pending_outcome == Outcome::Finish;
}

auto claim_duel_link(Engine::Core::Entity& attacker,
                     Engine::Core::Entity& target) -> std::optional<CombatActionId> {
  auto* duel = attacker.get_component<CommanderDuelComponent>();
  auto* theirs = target.get_component<CommanderDuelComponent>();
  if (duel == nullptr || theirs == nullptr || duel->opponent_id != target.get_id()) {
    return std::nullopt;
  }
  WeaponFamily const weapon = weapon_family_of(attacker);
  float const distance = planar_distance(attacker, target);
  constexpr float k_lunge_from = 2.3F;
  if (duel->clash_reply_requested) {
    duel->clash_reply_requested = false;
    duel->pending_outcome = Outcome::ClashReply;
    return action_for(distance > k_lunge_from ? LinkMove::Lunge : LinkMove::Heavy,
                      weapon);
  }
  if (!duel_forces_swing(attacker)) {
    return std::nullopt;
  }

  auto const& string = k_strings[duel->round % k_strings.size()];
  Link link = string[std::min<std::size_t>(duel->link, string.size() - 1U)];
  auto const* target_unit = target.get_component<Engine::Core::UnitComponent>();
  bool const execution =
      target_unit != nullptr &&
      static_cast<float>(target_unit->health) <=
          static_cast<float>(target_unit->max_health) * k_execution_health_fraction;
  if (execution) {
    link = {distance > k_lunge_from ? LinkMove::Lunge : LinkMove::Finisher,
            Outcome::Finish};
    duel->round_decided = true;
  } else {
    if (link.outcome == Outcome::Evade && !has_room_to_evade(target)) {
      link.outcome = Outcome::Parry;
    }
    ++duel->link;
    duel->round_decided =
        duel->link >= string.size() || link.outcome == Outcome::Evade ||
        (link.outcome == Outcome::Finish && link.move != LinkMove::Lunge);
  }
  duel->pending_outcome = link.outcome;
  theirs->evade_started = false;
  theirs->clash_reply_requested = link.outcome == Outcome::Clash;
  rise_for_air_link(attacker, link.move);
  return action_for(link.move, weapon);
}

auto duel_swing_cadence(const Engine::Core::Entity& attacker,
                        float cooldown,
                        float link_length) -> std::optional<float> {
  if (!attacker.has_component<CommanderDuelComponent>()) {
    return std::nullopt;
  }
  return cooldown - std::max(0.05F, link_length);
}

auto resolve_duel_contact(Engine::Core::Entity& attacker,
                          Engine::Core::Entity& target,
                          int base_damage) -> std::optional<DuelContact> {
  auto const* duel = attacker.get_component<CommanderDuelComponent>();
  if (duel == nullptr || duel->opponent_id != target.get_id() ||
      duel->pending_outcome == Outcome::None ||
      !target.has_component<CommanderDuelComponent>()) {
    return std::nullopt;
  }
  DuelContact contact{.outcome = duel->pending_outcome};
  float const multiplier = outcome_damage_multiplier(contact.outcome);
  contact.damage = multiplier > 0.0F
                       ? std::max(1,
                                  static_cast<int>(std::lround(
                                      static_cast<float>(base_damage) * multiplier)))
                       : 0;
  auto const* unit = target.get_component<Engine::Core::UnitComponent>();
  if (contact.outcome != Outcome::Finish && unit != nullptr) {
    contact.damage = std::clamp(contact.damage, 0, std::max(0, unit->health - 1));
  }
  return contact;
}

void present_duel_contact(Engine::Core::World& world,
                          Engine::Core::Entity& attacker,
                          Engine::Core::Entity& target,
                          const Engine::Core::TransformComponent& attacker_transform,
                          const Engine::Core::TransformComponent& target_transform,
                          const DuelContact& contact) {
  auto* duel = attacker.get_component<CommanderDuelComponent>();
  auto* theirs = target.get_component<CommanderDuelComponent>();
  if (duel == nullptr || theirs == nullptr) {
    return;
  }
  float dx = target_transform.position.x - attacker_transform.position.x;
  float dz = target_transform.position.z - attacker_transform.position.z;
  float const distance = std::max(0.001F, std::hypot(dx, dz));
  dx /= distance;
  dz /= distance;
  QVector3D const meeting_point(attacker_transform.position.x + dx * distance * 0.55F,
                                target_transform.position.y + 1.15F,
                                attacker_transform.position.z + dz * distance * 0.55F);
  auto const target_alive = [&target] {
    auto const* unit = target.get_component<Engine::Core::UnitComponent>();
    return unit != nullptr && unit->health > 0;
  };
  switch (contact.outcome) {
  case Outcome::Parry:
    play_cue(Game::Audio::Cue::k_combat_block);
    apply_melee_reaction_feedback(
        &world, &target, attacker.get_id(), Engine::Core::HitReactionKind::Block);
    queue_melee_contact_burst(
        target, meeting_point, Engine::Core::RpgContactOutcome::Block, 1.1F);
    slide(*theirs, dx, dz, 2.6F);
    break;
  case Outcome::Hit:
    queue_melee_contact_burst(
        target, meeting_point, Engine::Core::RpgContactOutcome::Damage, 1.0F);
    slide(*theirs, dx, dz, 3.4F);
    break;
  case Outcome::Clash:
    play_cue(Game::Audio::Cue::k_combat_perfect_guard);
    queue_melee_contact_burst(
        target, meeting_point, Engine::Core::RpgContactOutcome::PerfectGuard, 1.8F);
    apply_hit_feedback(
        &target, attacker.get_id(), &world, Engine::Core::HitReactionKind::Recoil);
    apply_hit_feedback(
        &attacker, target.get_id(), &world, Engine::Core::HitReactionKind::Recoil);
    slide(*theirs, dx, dz, 4.6F);
    slide(*duel, -dx, -dz, 4.6F);
    duel->swing_delay = 0.35F;
    break;
  case Outcome::Launch:
    if (target_alive()) {
      launch(target, target_transform, dx, dz, 8.5F);
      theirs->tumble_remaining = CommanderDuelComponent::k_tumble_seconds;
      add_or_extend_stagger(&target, 1.0F, Engine::Core::StaggerTier::Knockback);
    }
    queue_melee_contact_burst(
        target, meeting_point, Engine::Core::RpgContactOutcome::Damage, 1.3F);
    break;
  case Outcome::Juggle:
    if (target_alive()) {
      launch(target, target_transform, dx, dz, 5.2F);
      add_or_extend_stagger(&target, 0.7F, Engine::Core::StaggerTier::Knockback);
    }
    queue_melee_contact_burst(
        target, meeting_point, Engine::Core::RpgContactOutcome::Damage, 1.1F);
    break;
  case Outcome::Finish:
    play_cue(Game::Audio::Cue::k_combat_stagger);
    if (target_alive()) {
      if (auto* airborne =
              target.get_component<Engine::Core::CombatLaunchComponent>()) {
        airborne->velocity_y = std::min(airborne->velocity_y, -9.0F);
      } else {
        launch(target, target_transform, dx, dz, 4.4F);
      }
      add_or_extend_stagger(&target, 0.85F, Engine::Core::StaggerTier::Knockback);
      apply_hit_feedback(
          &target, attacker.get_id(), &world, Engine::Core::HitReactionKind::Stagger);
      slide(*theirs, dx, dz, 10.5F);
      theirs->tumble_remaining = CommanderDuelComponent::k_tumble_seconds;
    }
    queue_melee_contact_burst(
        target, meeting_point, Engine::Core::RpgContactOutcome::Damage, 1.6F);
    break;
  case Outcome::None:
  case Outcome::Evade:
  case Outcome::ClashReply:
    break;
  }
}

} // namespace Game::Systems::Combat
