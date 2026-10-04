#include "arrow_volley.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../../core/component.h"
#include "../../units/commander_catalog.h"
#include "../../units/troop_config.h"
#include "../../visuals/team_colors.h"
#include "../combat_rules.h"
#include "../projectile_system.h"
#include "../rpg_combat_system/rpg_commander_damage.h"
#include "../rpg_combat_system/rpg_targeting.h"
#include "combat_random.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "structure_combat.h"

namespace Game::Systems::Combat {

namespace {

struct VolleyStyle {
  ArrowVisualStyle style = ArrowVisualStyle::Volley;
  float lateral_scale = 1.0F;
  int arrow_count = 1;
};

struct VolleyGeometry {
  QVector3D attacker_pos;
  QVector3D target_pos;
  QVector3D direction;
  QVector3D color;
  bool target_is_structure = false;
  StructureAttackProfile structure_profile;
};

auto volley_color(const Engine::Core::UnitComponent* attacker_unit) -> QVector3D {
  return attacker_unit != nullptr
             ? Game::Visuals::team_colorForOwner(attacker_unit->owner_id)
             : QVector3D(0.8F, 0.9F, 1.0F);
}

struct MissileProfile {
  ProjectileKind kind{ProjectileKind::Arrow};
  ArrowVisualStyle style{ArrowVisualStyle::Focused};
  float speed{Constants::k_arrow_speed};
  float arc_scale{1.0F};
};

auto missile_profile(const Engine::Core::Entity& attacker,
                     ArrowVisualStyle style) -> MissileProfile {
  auto const* registry = attacker.registry();
  auto const* unit =
      registry == nullptr
          ? nullptr
          : registry->try_get<Engine::Core::UnitComponent>(attacker.get_id());
  if (unit != nullptr && unit->spawn_type == Game::Units::SpawnType::Slinger) {
    return {ProjectileKind::SlingStone, ArrowVisualStyle::Focused, 18.0F, 0.18F};
  }
  if (unit != nullptr && unit->spawn_type == Game::Units::SpawnType::Velites) {
    return {ProjectileKind::Javelin, ArrowVisualStyle::Javelin, 12.0F, 1.0F};
  }
  return {ProjectileKind::Arrow, style, Constants::k_arrow_speed, 1.0F};
}

void spawn_focused_arrow_at_rpg_target(Engine::Core::Entity* attacker,
                                       Engine::Core::Entity* target,
                                       ProjectileSystem* projectile_sys,
                                       const QVector3D& a_pos,
                                       const QVector3D& t_pos,
                                       const QVector3D& color,
                                       int damage) {
  QVector3D source_pos = a_pos;
  if (auto const damage_carrier =
          Game::Systems::RpgCombat::resolve_damage_carrier(*attacker, target->get_id());
      damage_carrier.has_value()) {
    source_pos = damage_carrier->position;
  }

  QVector3D direction = t_pos - source_pos;
  if (direction.lengthSquared() <= 1.0e-6F) {
    direction = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    direction.normalize();
  }
  QVector3D const start = source_pos + QVector3D(0.0F, 1.18F, 0.0F) +
                          direction * Constants::k_arrow_start_offset;
  QVector3D const end = t_pos + QVector3D(0.0F, 1.25F, 0.0F) - direction * 0.42F;
  auto const missile = missile_profile(*attacker, ArrowVisualStyle::Focused);
  projectile_sys->spawn_arrow(start,
                              end,
                              color,
                              missile.speed,
                              false,
                              missile.kind,
                              true,
                              std::max(1, damage),
                              attacker->get_id(),
                              target->get_id(),
                              0.0F,
                              0.0F,
                              false,
                              missile.style,
                              t_pos,
                              missile.arc_scale);
}

auto crowd_arrow_count(const Engine::Core::Entity* attacker,
                       const Engine::Core::Entity* target,
                       const Engine::Core::UnitComponent* attacker_unit) -> int {
  int arrow_count = 1;
  if (attacker_unit != nullptr) {
    int const troop_size =
        Game::Units::TroopConfig::instance().get_individuals_per_unit(
            attacker_unit->spawn_type);

    int const max_arrows = std::max(2, (troop_size * 2) / 3);

    std::uint32_t const seed =
        static_cast<std::uint32_t>(attacker->get_id() * 2246822519U) ^
        static_cast<std::uint32_t>(target->get_id() * 3266489917U) ^
        static_cast<std::uint32_t>(troop_size * 0x9E37U);
    int const min_arrows = max_arrows / 2;
    int const range = std::max(1, max_arrows - min_arrows + 1);
    arrow_count =
        min_arrows + std::min(range - 1,
                              static_cast<int>(deterministic_unit_roll(seed, 3U) *
                                               static_cast<float>(range)));
  }
  return std::min(arrow_count, Constants::k_max_visual_arrows_per_volley);
}

auto resolve_volley_style(const Engine::Core::Entity* attacker,
                          const Engine::Core::Entity* target,
                          const Engine::Core::UnitComponent* attacker_unit)
    -> VolleyStyle {
  VolleyStyle volley;
  volley.arrow_count = crowd_arrow_count(attacker, target, attacker_unit);
  if (auto const* commander =
          attacker->get_component<Engine::Core::CommanderComponent>();
      commander != nullptr && !commander->fpv_controlled) {
    bool const signature_shot = commander->signature_strike_active;
    bool const volley_signature =
        signature_shot &&
        commander->signature_move ==
            static_cast<std::uint8_t>(
                Game::Units::CommanderSignatureMove::PointBlankVolley);
    volley.style = signature_shot ? ArrowVisualStyle::CommanderSignature
                                  : ArrowVisualStyle::Commander;
    volley.arrow_count = volley_signature ? 3 : 1;
    volley.lateral_scale = 0.55F;
  }
  return volley;
}

void spawn_volley_arrow(Engine::Core::Entity* attacker,
                        Engine::Core::Entity* target,
                        ProjectileSystem* projectile_sys,
                        const VolleyGeometry& geometry,
                        const VolleyStyle& volley,
                        int index,
                        int damage) {
  QVector3D const& a_pos = geometry.attacker_pos;
  QVector3D const& t_pos = geometry.target_pos;
  QVector3D const& dir = geometry.direction;
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  QVector3D const up_vector(0.0F, 1.0F, 0.0F);
  int const wave_count = std::max(1, Constants::k_arrow_volley_wave_count);
  int const rank_count =
      std::max(1, (volley.arrow_count + wave_count - 1) / wave_count);
  int const i = index;

  std::uint32_t const spread_seed =
      static_cast<std::uint32_t>(attacker->get_id() * 2246822519U) ^
      static_cast<std::uint32_t>(target->get_id() * 3266489917U) ^
      static_cast<std::uint32_t>((i + 1) * 0x85EBCA6BU);
  float const spread_a = deterministic_range(
      spread_seed, 11U, Constants::k_arrow_spread_min, Constants::k_arrow_spread_max);
  float const spread_b = deterministic_range(
      spread_seed, 12U, Constants::k_arrow_spread_min, Constants::k_arrow_spread_max);
  float const spread_c = deterministic_range(
      spread_seed, 13U, Constants::k_arrow_spread_min, Constants::k_arrow_spread_max);
  int const wave_index = i % wave_count;
  int const rank_index = i / wave_count;
  float const centered_wave =
      static_cast<float>(wave_index) - (static_cast<float>(wave_count - 1) * 0.5F);
  float const centered_rank =
      static_cast<float>(rank_index) - (static_cast<float>(rank_count - 1) * 0.5F);

  float const base_lateral = centered_rank * Constants::k_arrow_volley_rank_spacing;
  float const launch_lateral = (base_lateral + spread_a * 0.60F) * volley.lateral_scale;
  float const target_lateral =
      ((base_lateral * 0.95F) + spread_b * 0.45F) * volley.lateral_scale;
  float const wave_height =
      (1.0F - 0.18F * std::abs(centered_wave)) * Constants::k_arrow_volley_wave_height;
  float const launch_height =
      wave_height +
      std::abs(spread_b) * (Constants::k_arrow_vertical_spread_factor * 0.48F);
  float const target_height =
      (wave_height * 0.5F) +
      std::abs(spread_c) * (Constants::k_arrow_vertical_spread_factor * 0.22F);
  float const wave_depth = centered_wave * Constants::k_arrow_volley_wave_depth;
  float const depth_offset =
      wave_depth + spread_c * (Constants::k_arrow_depth_spread_factor * 0.55F);

  QVector3D const start_offset =
      perpendicular * launch_lateral + up_vector * launch_height;
  QVector3D const end_offset =
      perpendicular * target_lateral + up_vector * target_height + dir * depth_offset;

  QVector3D const start = a_pos +
                          QVector3D(0.0F, Constants::k_arrow_start_height, 0.0F) +
                          dir * Constants::k_arrow_start_offset + start_offset;
  QVector3D const end =
      geometry.target_is_structure
          ? structure_impact_point(*target,
                                   a_pos,
                                   target_lateral,
                                   geometry.structure_profile.impact_height +
                                       target_height * 0.35F)
          : t_pos + dir * Constants::k_arrow_target_offset +
                QVector3D(0.0F, Constants::k_arrow_target_offset, 0.0F) + end_offset;

  bool const damage_carrier = i == volley.arrow_count / 2;
  auto const missile = missile_profile(*attacker, volley.style);
  projectile_sys->spawn_arrow(start,
                              end,
                              geometry.color,
                              missile.speed,
                              false,
                              missile.kind,
                              damage_carrier,
                              damage_carrier ? std::max(1, damage) : 0,
                              attacker->get_id(),
                              target->get_id(),
                              0.0F,
                              0.0F,
                              false,
                              missile.style,
                              t_pos,
                              missile.arc_scale);
}

} // namespace

void spawn_rts_arrow_volley(Engine::Core::Entity* attacker,
                            Engine::Core::Entity* target,
                            ProjectileSystem* projectile_sys,
                            int damage) {
  if (projectile_sys == nullptr) {
    return;
  }

  auto* att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto* tgt_t = target->get_component<Engine::Core::TransformComponent>();
  auto* att_u = attacker->get_component<Engine::Core::UnitComponent>();

  if ((att_t == nullptr) || (tgt_t == nullptr)) {
    return;
  }

  VolleyGeometry geometry;
  geometry.attacker_pos =
      QVector3D(att_t->position.x, att_t->position.y, att_t->position.z);
  geometry.target_pos =
      QVector3D(tgt_t->position.x, tgt_t->position.y, tgt_t->position.z);
  geometry.target_is_structure = is_building(target);
  geometry.color = volley_color(att_u);

  if (Game::Systems::CombatRules::uses_rpg_combat_rules(target)) {
    spawn_focused_arrow_at_rpg_target(attacker,
                                      target,
                                      projectile_sys,
                                      geometry.attacker_pos,
                                      geometry.target_pos,
                                      geometry.color,
                                      damage);
    return;
  }

  geometry.structure_profile = structure_attack_profile(attacker);
  QVector3D const central_aim =
      geometry.target_is_structure
          ? structure_impact_point(*target,
                                   geometry.attacker_pos,
                                   0.0F,
                                   geometry.structure_profile.impact_height)
          : geometry.target_pos;
  geometry.direction = (central_aim - geometry.attacker_pos).normalized();

  VolleyStyle const volley = resolve_volley_style(attacker, target, att_u);
  for (int i = 0; i < volley.arrow_count; ++i) {
    spawn_volley_arrow(attacker, target, projectile_sys, geometry, volley, i, damage);
  }
}

} // namespace Game::Systems::Combat
