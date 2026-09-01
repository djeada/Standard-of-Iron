#include "combat_dust_renderer.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system/structure_combat.h"
#include "game/systems/combat_system/structure_fire.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "render/camera_visibility.h"
#include "render/combat_dust_defaults.h"
#include "render/scene_renderer.h"

namespace Render::GL {

namespace {

auto commander_strike_accent(Engine::Core::Entity* commander) -> QVector3D {
  auto const* unit = commander->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return {1.0F, 0.78F, 0.32F};
  }
  switch (unit->nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return {1.0F, 0.80F, 0.30F};
  case Game::Systems::NationID::Carthage:
    return {0.86F, 0.46F, 1.0F};
  default:
    return {1.0F, 0.72F, 0.24F};
  }
}

void draw_commander_swing(
    Render::GL::Renderer* renderer,
    const Engine::Core::CommanderSignaturePresentationComponent::Entry& entry,
    const QVector3D& contact,
    const QVector3D& forward,
    const QVector3D& accent,
    float progress) {
  constexpr float k_cut_tilt = 0.62F;
  constexpr float k_slam_tilt = 1.02F;
  float const reach = std::max(1.2F, entry.reach);
  float const arc_alpha = entry.intensity * 0.85F;
  QVector3D const hot = accent * 0.22F + QVector3D(0.70F, 0.72F, 0.74F);

  switch (entry.form) {
  case Engine::Core::CommanderSignatureForm::Cut: {
    float const tilt = entry.span < 0.0F ? -k_cut_tilt : k_cut_tilt;
    renderer->weapon_arc(contact,
                         hot,
                         reach * 1.05F,
                         1.15F * arc_alpha,
                         progress,
                         forward,
                         entry.span,
                         tilt);
    break;
  }
  case Engine::Core::CommanderSignatureForm::Sweep: {
    renderer->weapon_arc(contact - QVector3D(0.0F, 0.22F, 0.0F),
                         hot,
                         reach * 0.92F,
                         1.0F * arc_alpha,
                         progress,
                         forward,
                         entry.span,
                         0.0F);
    break;
  }
  case Engine::Core::CommanderSignatureForm::Slam: {
    renderer->weapon_arc(contact - forward * (reach * 0.12F),
                         hot,
                         reach * 0.95F,
                         1.2F * arc_alpha,
                         progress,
                         forward,
                         std::abs(entry.span) * 0.8F,
                         k_slam_tilt);
    constexpr float k_shock_start = 0.34F;
    if (progress >= k_shock_start) {
      float const shock =
          std::clamp((progress - k_shock_start) / (1.0F - k_shock_start), 0.0F, 1.0F);
      QVector3D const ground(contact.x(), contact.y() - 0.90F, contact.z());
      renderer->weapon_arc(ground + forward * (reach * 0.40F),
                           accent,
                           reach * 0.80F,
                           0.55F * arc_alpha,
                           shock,
                           forward,
                           1.0F,
                           0.0F);
      renderer->combat_dust(ground + forward * (reach * 0.40F),
                            QVector3D(0.55F, 0.47F, 0.36F),
                            0.7F + 0.8F * shock,
                            0.5F * (1.0F - shock) * arc_alpha,
                            renderer->get_animation_time());
    }
    break;
  }
  case Engine::Core::CommanderSignatureForm::Thrust: {
    float const drive = std::clamp(progress * 1.6F, 0.0F, 1.0F);
    QVector3D const tip = contact + forward * (reach * (0.15F + 0.55F * drive));
    renderer->metal_spark(
        tip, hot, 0.12F, 1.4F * arc_alpha, std::max(0.0F, entry.age), forward);
    renderer->metal_spark(contact + forward * (reach * 0.25F * drive),
                          accent,
                          0.09F,
                          0.9F * arc_alpha,
                          std::max(0.0F, entry.age - 0.04F),
                          forward);
    break;
  }
  case Engine::Core::CommanderSignatureForm::Shot:
    break;
  }

  Render::LocalLight swing_light;
  swing_light.position = contact + forward * (reach * 0.3F);
  swing_light.color = accent * 0.25F + QVector3D(0.48F, 0.50F, 0.52F);
  swing_light.radius = 1.4F + reach * 0.35F;
  swing_light.intensity =
      0.22F * arc_alpha * (1.0F - progress) * std::clamp(progress * 4.0F, 0.0F, 1.0F);
  renderer->local_light(swing_light);
}

constexpr float k_degrees_to_radians = 0.017453292519943295F;
constexpr float k_dust_y_offset = 0.05F;
constexpr float k_dust_color_r = 0.64F;
constexpr float k_dust_color_g = 0.54F;
constexpr float k_dust_color_b = 0.39F;
constexpr float k_visibility_check_radius = 3.0F;

constexpr float k_flame_radius = 3.0F;
constexpr float k_flame_intensity = 0.8F;
constexpr float k_flame_y_offset = 0.5F;
constexpr float k_flame_color_r = 1.0F;
constexpr float k_flame_color_g = 0.4F;
constexpr float k_flame_color_b = 0.1F;
constexpr float k_fire_patch_flame_y_offset = 0.10F;
constexpr float k_fire_patch_flame_radius_scale = 0.28F;
constexpr float k_fire_patch_flame_min_radius = 0.26F;
constexpr float k_fire_patch_flame_offset_scale = 0.36F;
constexpr float k_burning_flame_intensity = 0.88F;
constexpr float k_burning_min_scale = 1.0F;
constexpr float k_burning_body_height = 0.74F;
constexpr float k_burning_lower_height = 0.38F;
constexpr float k_burning_side_offset = 0.28F;
constexpr float k_burning_front_offset = 0.22F;
constexpr float k_burning_body_radius = 0.27F;
constexpr float k_burning_lower_radius = 0.22F;
constexpr float k_burning_shoulder_radius = 0.20F;

constexpr float k_blood_y_offset = 0.02F;

constexpr float k_elephant_stomp_impact_radius = 0.52F;
constexpr float k_stone_impact_intensity = 1.5F;
constexpr float k_stone_impact_y_offset = 0.1F;
constexpr float k_stone_impact_duration = 10.0F;
struct UnitFlameAnchor {
  float local_x{0.0F};
  float local_y{0.0F};
  float local_z{0.0F};
  float radius{0.5F};
  float intensity{1.0F};
  float time_offset{0.0F};
  QVector3D color{k_flame_color_r, k_flame_color_g, k_flame_color_b};
};

struct GroundFlameLobe {
  float local_x{0.0F};
  float local_z{0.0F};
  float radius_scale{1.0F};
  float intensity_scale{1.0F};
  float time_offset{0.0F};
};

auto blood_alpha_scale(float elapsed_time, float lifetime) -> float {
  if (lifetime <= 0.0F) {
    return 0.0F;
  }

  float const fade_window = std::max(1.0F, lifetime * 0.25F);
  float const remaining_time = lifetime - elapsed_time;
  if (remaining_time >= fade_window) {
    return 1.0F;
  }
  return std::clamp(remaining_time / fade_window, 0.0F, 1.0F);
}

auto transform_unit_anchor(const Engine::Core::TransformComponent& transform,
                           const UnitFlameAnchor& anchor,
                           float size_scale) -> QVector3D {
  float const yaw = transform.rotation.y * k_degrees_to_radians;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  float const local_x = anchor.local_x * size_scale;
  float const local_z = anchor.local_z * size_scale;
  float const world_x = transform.position.x + local_x * cos_yaw + local_z * sin_yaw;
  float const world_z = transform.position.z - local_x * sin_yaw + local_z * cos_yaw;
  float const world_y = transform.position.y + anchor.local_y * size_scale;
  return {world_x, world_y, world_z};
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit_comp)
    -> int {
  return Game::Systems::FormationCombat::resolve_definition(unit_comp).total_count;
}
} // namespace

auto StoneImpactTracker::instance() -> StoneImpactTracker& {
  static StoneImpactTracker instance;
  return instance;
}

void StoneImpactTracker::add_impact(const QVector3D& position,
                                    float current_time,
                                    float radius,
                                    float intensity,
                                    const QVector3D& color) {
  StoneImpactEffect effect;
  effect.position = position;
  effect.color = color;
  effect.start_time = current_time;
  effect.duration = k_stone_impact_duration;
  effect.radius = radius;
  effect.intensity = intensity;
  m_impacts.push_back(effect);
}

void StoneImpactTracker::update(float current_time) {
  m_impacts.erase(std::remove_if(m_impacts.begin(),
                                 m_impacts.end(),
                                 [current_time](const StoneImpactEffect& impact) {
                                   return (current_time - impact.start_time) >
                                          impact.duration;
                                 }),
                  m_impacts.end());
}

void render_blood_stains(Renderer* renderer,
                         ResourceManager*,
                         Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  auto& visibility = Render::GL::CameraVisibility::instance();
  auto const& world_view = renderer->world_view();
  auto fog_snapshot =
      world_view.has_visibility() ? world_view.visibility()->snapshot_ptr() : nullptr;
  auto is_fog_visible = [&fog_snapshot](float world_x, float world_z) -> bool {
    return fog_snapshot == nullptr ||
           Game::Map::should_render_combat_effect(*fog_snapshot, world_x, world_z);
  };

  auto blood_stains = world->collect_entities_with<Engine::Core::BloodStainComponent>();
  for (auto* blood_entity : blood_stains) {
    if (blood_entity == nullptr ||
        blood_entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = blood_entity->get_component<Engine::Core::TransformComponent>();
    auto* blood_stain =
        blood_entity->get_component<Engine::Core::BloodStainComponent>();
    if (transform == nullptr || blood_stain == nullptr) {
      continue;
    }

    float const alpha_scale =
        blood_alpha_scale(blood_stain->elapsed_time, blood_stain->lifetime);
    if (alpha_scale <= 0.0F) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    float const visibility_radius =
        std::max(k_visibility_check_radius, blood_stain->radius);
    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, visibility_radius)) {
      continue;
    }

    QVector3D const position(transform->position.x,
                             transform->position.y + k_blood_y_offset,
                             transform->position.z);
    renderer->blood_pool(position,
                         blood_stain->radius,
                         alpha_scale,
                         blood_stain->rotation,
                         blood_stain->aspect_ratio,
                         blood_stain->seed);
  }
}

struct GroundDustCluster {
  float sum_x{0.0F};
  float sum_z{0.0F};
  float weight{0.0F};
  int cell_x{0};
  int cell_z{0};
};

auto dust_cell_of(float world_value) -> int {
  return static_cast<int>(
      std::floor(world_value / Render::CombatDustDefaults::k_cluster_cell));
}

auto dust_cell_phase(int cell_x, int cell_z) -> float {
  auto const mixed = static_cast<unsigned int>(cell_x * 73856093 ^ cell_z * 19349663);
  return static_cast<float>(mixed % 1024U) * (1.0F / 1024.0F) * 12.0F;
}

void accumulate_ground_dust(std::vector<GroundDustCluster>& clusters,
                            float world_x,
                            float world_z) {
  int const cell_x = dust_cell_of(world_x);
  int const cell_z = dust_cell_of(world_z);
  for (auto& cluster : clusters) {
    if (cluster.cell_x == cell_x && cluster.cell_z == cell_z) {
      cluster.sum_x += world_x;
      cluster.sum_z += world_z;
      cluster.weight += 1.0F;
      return;
    }
  }
  clusters.push_back({world_x, world_z, 1.0F, cell_x, cell_z});
}

void emit_ground_dust(Renderer* renderer,
                      std::vector<GroundDustCluster>& clusters,
                      float animation_time) {
  namespace Defaults = Render::CombatDustDefaults;

  std::sort(clusters.begin(),
            clusters.end(),
            [](const GroundDustCluster& lhs, const GroundDustCluster& rhs) {
              if (lhs.weight != rhs.weight) {
                return lhs.weight > rhs.weight;
              }
              if (lhs.cell_x != rhs.cell_x) {
                return lhs.cell_x < rhs.cell_x;
              }
              return lhs.cell_z < rhs.cell_z;
            });

  auto const kept = std::min<std::size_t>(clusters.size(), Defaults::k_max_clusters);
  for (std::size_t index = 0; index < kept; ++index) {
    const GroundDustCluster& cluster = clusters[index];
    float const crowd = std::sqrt(std::max(0.0F, cluster.weight - 1.0F));
    float const radius =
        std::min(Defaults::k_cluster_radius_max,
                 Defaults::k_radius + Defaults::k_cluster_radius_growth * crowd);
    float const intensity =
        std::min(Defaults::k_cluster_intensity_max,
                 Defaults::k_intensity + Defaults::k_cluster_intensity_growth * crowd);

    QVector3D const position(cluster.sum_x / cluster.weight,
                             k_dust_y_offset,
                             cluster.sum_z / cluster.weight);
    QVector3D const color(k_dust_color_r, k_dust_color_g, k_dust_color_b);
    renderer->combat_dust(position,
                          color,
                          radius,
                          intensity,
                          animation_time +
                              dust_cell_phase(cluster.cell_x, cluster.cell_z));
  }
}

void render_combat_dust(Renderer* renderer,
                        ResourceManager*,
                        Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float const animation_time = renderer->get_animation_time();
  auto& visibility = Render::GL::CameraVisibility::instance();
  auto const& world_view = renderer->world_view();
  auto fog_snapshot =
      world_view.has_visibility() ? world_view.visibility()->snapshot_ptr() : nullptr;
  auto is_fog_visible = [&fog_snapshot](float world_x, float world_z) -> bool {
    return fog_snapshot == nullptr ||
           Game::Map::should_render_combat_effect(*fog_snapshot, world_x, world_z);
  };

  std::vector<GroundDustCluster> ground_dust;

  auto units = world->collect_entities_with<Engine::Core::AttackComponent>();

  for (auto* unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = unit->get_component<Engine::Core::TransformComponent>();
    auto* attack = unit->get_component<Engine::Core::AttackComponent>();
    auto* unit_comp = unit->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || attack == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (!attack->in_melee_lock ||
        !Game::Systems::CombatRules::participates_in_rts_melee_lock(unit)) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    accumulate_ground_dust(ground_dust, transform->position.x, transform->position.z);
  }

  auto builders =
      world->collect_entities_with<Engine::Core::BuilderProductionComponent>();

  for (auto* builder : builders) {
    if (builder->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = builder->get_component<Engine::Core::TransformComponent>();
    auto* production =
        builder->get_component<Engine::Core::BuilderProductionComponent>();
    auto* unit_comp = builder->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || production == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (!production->in_progress) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    accumulate_ground_dust(ground_dust, transform->position.x, transform->position.z);
  }

  emit_ground_dust(renderer, ground_dust, animation_time);

  auto burning_structures =
      world->collect_entities_with<Engine::Core::StructureFireComponent>();

  for (auto* building : burning_structures) {
    if (building == nullptr ||
        building->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    float const fire_intensity =
        Game::Systems::Combat::structure_fire_intensity(*building);
    if (fire_intensity <= 0.0F) {
      continue;
    }

    auto* transform = building->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    QVector3D const position(
        transform->position.x, k_flame_y_offset, transform->position.z);
    QVector3D const color(k_flame_color_r, k_flame_color_g, k_flame_color_b);

    renderer->building_flame(position,
                             color,
                             k_flame_radius,
                             k_flame_intensity * fire_intensity,
                             animation_time);
  }

  auto structure_impacts =
      world
          ->collect_entities_with<Engine::Core::StructureDamagePresentationComponent>();
  for (auto* building : structure_impacts) {
    if (building == nullptr ||
        building->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto const* impacts =
        building->get_component<Engine::Core::StructureDamagePresentationComponent>();
    if (impacts == nullptr) {
      continue;
    }
    for (auto const& impact : impacts->impacts) {
      if (impact.lifetime <= 0.0F || impact.age >= impact.lifetime ||
          !is_fog_visible(impact.x, impact.z) ||
          !visibility.is_entity_visible(
              impact.x, impact.z, std::max(k_visibility_check_radius, impact.radius))) {
        continue;
      }

      QVector3D color(0.58F, 0.52F, 0.43F);
      using Game::Systems::Combat::StructureImpactStyle;
      switch (static_cast<StructureImpactStyle>(impact.style)) {
      case StructureImpactStyle::LightMelee:
        color = {0.64F, 0.58F, 0.47F};
        break;
      case StructureImpactStyle::HeavyMelee:
        color = {0.60F, 0.54F, 0.44F};
        break;
      case StructureImpactStyle::Elephant:
        color = {0.68F, 0.58F, 0.43F};
        break;
      case StructureImpactStyle::Ballista:
        color = {0.76F, 0.67F, 0.45F};
        break;
      case StructureImpactStyle::Catapult:
        color = {0.54F, 0.50F, 0.45F};
        break;
      case StructureImpactStyle::Magic:
        color = {0.72F, 0.28F, 0.88F};
        break;
      }

      QVector3D const position(impact.x, impact.y, impact.z);
      renderer->stone_impact(
          position, color, impact.radius, impact.intensity, impact.age);
      float const dust_fade =
          std::clamp(1.0F - impact.age / std::min(impact.lifetime, 0.65F), 0.0F, 1.0F);
      if (dust_fade > 0.0F) {
        QVector3D const dust_position(impact.x + impact.normal_x * 0.08F,
                                      impact.y,
                                      impact.z + impact.normal_z * 0.08F);
        renderer->combat_dust(dust_position,
                              color,
                              impact.radius * 1.25F,
                              impact.intensity * dust_fade,
                              animation_time + impact.x * 0.17F + impact.z * 0.11F);
      }
    }
  }

  auto rpg_contacts =
      world->collect_entities_with<Engine::Core::RpgContactPresentationComponent>();
  for (auto* entity : rpg_contacts) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto const* presentation =
        entity->get_component<Engine::Core::RpgContactPresentationComponent>();
    if (presentation == nullptr) {
      continue;
    }
    for (auto const& contact : presentation->entries) {
      if (contact.lifetime <= 0.0F || contact.age >= contact.lifetime ||
          !is_fog_visible(contact.x, contact.z) ||
          !visibility.is_entity_visible(contact.x, contact.z, 2.0F)) {
        continue;
      }
      QVector3D color(0.86F, 0.16F, 0.08F);
      float radius = 0.22F;
      float intensity_scale = 0.58F;
      switch (contact.outcome) {
      case Engine::Core::RpgContactOutcome::Block:
        color = {0.96F, 0.74F, 0.28F};
        radius = 0.30F;
        intensity_scale = 0.80F;
        break;
      case Engine::Core::RpgContactOutcome::PerfectGuard:
        color = {0.82F, 0.96F, 1.0F};
        radius = 0.42F;
        intensity_scale = 1.05F;
        break;
      case Engine::Core::RpgContactOutcome::Dodge:
        color = {0.34F, 0.78F, 0.92F};
        radius = 0.18F;
        intensity_scale = 0.48F;
        break;
      case Engine::Core::RpgContactOutcome::Damage:
        break;
      }
      renderer->metal_spark(QVector3D(contact.x, contact.y, contact.z),
                            color,
                            radius,
                            contact.intensity * intensity_scale,
                            contact.age);
    }
  }

  auto casters =
      world->collect_entities_with<Engine::Core::CreaturePresentationComponent>();
  for (auto* caster : casters) {
    if (caster == nullptr ||
        caster->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto const* presentation =
        caster->get_component<Engine::Core::CreaturePresentationComponent>();
    auto* transform = caster->get_component<Engine::Core::TransformComponent>();
    auto const* unit_comp = caster->get_component<Engine::Core::UnitComponent>();
    if (presentation == nullptr || transform == nullptr || !presentation->is_casting ||
        presentation->cast != Engine::Core::CreatureCastPresentation::Fireball) {
      continue;
    }
    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z) ||
        !visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    float const charge = std::clamp(presentation->combat_phase_progress, 0.0F, 1.0F);
    float const swell = std::sin(charge * std::numbers::pi_v<float>);
    if (swell <= 0.02F) {
      continue;
    }

    float const size_scale = std::max(
        0.5F, std::max({transform->scale.x, transform->scale.y, transform->scale.z}));
    UnitFlameAnchor const hand{
        0.17F, 1.12F, 0.42F, 0.0F, 1.0F, 0.0F, QVector3D(1.0F, 0.5F, 0.1F)};
    QVector3D const hand_position = transform_unit_anchor(*transform, hand, size_scale);
    float const flicker =
        0.92F + 0.08F * std::sin(animation_time * 21.0F + transform->position.x);

    renderer->fireball(hand_position,
                       QVector3D(0.36F, 0.10F, 0.02F),
                       (0.085F + 0.115F * swell) * size_scale * flicker,
                       0.85F * swell,
                       animation_time * 1.6F);
    renderer->fireball(hand_position,
                       QVector3D(1.0F, 0.52F, 0.10F),
                       (0.048F + 0.075F * swell) * size_scale * flicker,
                       1.9F * swell,
                       animation_time * 2.3F + 1.7F);
    renderer->fireball(hand_position,
                       QVector3D(1.0F, 0.86F, 0.52F),
                       (0.018F + 0.032F * swell) * size_scale,
                       2.2F * swell * swell,
                       animation_time * 3.1F + 4.2F);

    Render::LocalLight charge_light;
    charge_light.position = hand_position;
    charge_light.color = QVector3D(1.0F, 0.46F, 0.15F);
    charge_light.radius = 2.1F;
    charge_light.intensity = 0.55F * swell;
    renderer->local_light(charge_light);
  }

  for (auto* commander : world->collect_entities_with<
                         Engine::Core::CommanderSignaturePresentationComponent>()) {
    if (commander == nullptr) {
      continue;
    }
    auto const* presentation =
        commander
            ->get_component<Engine::Core::CommanderSignaturePresentationComponent>();
    if (presentation == nullptr) {
      continue;
    }
    QVector3D const accent = commander_strike_accent(commander);

    for (auto const& entry : presentation->entries) {
      float const progress =
          std::clamp(entry.age / std::max(0.01F, entry.lifetime), 0.0F, 1.0F);
      float const fade = (1.0F - progress) * (1.0F - progress) * entry.intensity;
      if (fade <= 0.01F && entry.cue == Engine::Core::CommanderStrikeCue::Impact) {
        continue;
      }

      if (!is_fog_visible(entry.x, entry.z) ||
          !visibility.is_entity_visible(entry.x, entry.z, k_visibility_check_radius)) {
        continue;
      }

      QVector3D const contact(entry.x, entry.y, entry.z);
      QVector3D const forward(entry.dir_x, 0.0F, entry.dir_z);
      QVector3D const across(-entry.dir_z, 0.0F, entry.dir_x);

      if (entry.cue == Engine::Core::CommanderStrikeCue::Swing) {
        draw_commander_swing(renderer, entry, contact, forward, accent, progress);
        continue;
      }

      auto spark_age = [&entry](float delay) {
        return std::max(0.0F, entry.age - delay);
      };
      QVector3D const spark_tint = accent * 0.35F + QVector3D(0.65F, 0.62F, 0.55F);

      switch (entry.form) {
      case Engine::Core::CommanderSignatureForm::Thrust: {

        for (int step = 0; step < 2; ++step) {
          float const along = -0.10F + 0.20F * static_cast<float>(step);
          renderer->metal_spark(
              contact + forward * (along + progress * 0.22F) +
                  QVector3D(0.0F, 0.05F * static_cast<float>(step % 2) - 0.03F, 0.0F),
              QVector3D(0.86F, 0.92F, 1.0F),
              0.10F * (0.8F + 0.2F * entry.intensity),
              1.65F * entry.intensity,
              spark_age(0.025F * static_cast<float>(step)),
              forward);
        }
        renderer->metal_spark(contact + forward * 0.06F,
                              QVector3D(1.0F, 0.95F, 0.82F),
                              0.13F,
                              2.2F * entry.intensity,
                              spark_age(0.0F),
                              forward);
        break;
      }
      case Engine::Core::CommanderSignatureForm::Cut: {

        for (int step = 0; step < 3; ++step) {
          float const offset = -0.32F + 0.32F * static_cast<float>(step);
          float const arc = 0.10F - 0.30F * offset * offset;
          renderer->metal_spark(contact + across * offset * (1.0F + progress * 0.4F) +
                                    forward * arc +
                                    QVector3D(0.0F, 0.20F * offset, 0.0F),
                                spark_tint,
                                0.09F * (0.8F + 0.2F * entry.intensity),
                                1.55F * entry.intensity,
                                spark_age(0.035F * static_cast<float>(step)),
                                across);
        }
        renderer->combat_dust(QVector3D(entry.x, entry.y - 0.55F, entry.z),
                              QVector3D(0.52F, 0.44F, 0.34F),
                              0.65F + 0.45F * progress,
                              0.30F * fade,
                              animation_time);
        break;
      }
      case Engine::Core::CommanderSignatureForm::Sweep: {

        for (int step = 0; step < 4; ++step) {
          float const angle =
              (-0.75F + 0.50F * static_cast<float>(step)) * (1.0F + progress * 0.35F);
          QVector3D const ray = forward * std::cos(angle) + across * std::sin(angle);
          renderer->metal_spark(contact + ray * (0.25F + 0.30F * progress) +
                                    QVector3D(0.0F, -0.15F, 0.0F),
                                spark_tint,
                                0.095F,
                                1.5F * entry.intensity,
                                spark_age(0.02F * static_cast<float>(step)),
                                ray);
        }
        renderer->combat_dust(QVector3D(entry.x, entry.y - 0.55F, entry.z),
                              QVector3D(0.50F, 0.43F, 0.34F),
                              0.85F + 0.65F * progress,
                              0.36F * fade,
                              animation_time);
        break;
      }
      case Engine::Core::CommanderSignatureForm::Slam: {

        for (int step = 0; step < 4; ++step) {
          float const angle = static_cast<float>(step) * 1.571F + progress * 0.6F;
          QVector3D const ray = forward * std::cos(angle) + across * std::sin(angle);
          renderer->metal_spark(contact + ray * 0.18F + QVector3D(0.0F, -0.35F, 0.0F),
                                QVector3D(1.0F, 0.90F, 0.70F),
                                0.115F,
                                1.9F * entry.intensity,
                                spark_age(0.02F * static_cast<float>(step)),
                                ray + QVector3D(0.0F, 0.35F, 0.0F));
        }
        renderer->stone_impact(QVector3D(entry.x, entry.y - 0.58F, entry.z),
                               QVector3D(0.55F, 0.47F, 0.36F),
                               0.78F * entry.intensity,
                               0.85F * entry.intensity,
                               entry.age);
        break;
      }
      case Engine::Core::CommanderSignatureForm::Shot: {

        renderer->metal_spark(contact,
                              QVector3D(1.0F, 0.90F, 0.62F),
                              0.150F,
                              3.2F,
                              spark_age(0.0F),
                              forward);
        for (int step = -1; step <= 1; ++step) {
          float const spread = 0.14F * static_cast<float>(step);
          renderer->metal_spark(contact - forward * 0.16F + across * spread +
                                    QVector3D(0.0F, 0.06F * spread, 0.0F),
                                QVector3D(0.95F, 0.80F, 0.48F),
                                0.090F,
                                1.9F,
                                spark_age(0.045F),
                                forward);
        }
        break;
      }
      }

      Render::LocalLight strike_light;
      strike_light.position = contact;
      strike_light.color = entry.form == Engine::Core::CommanderSignatureForm::Thrust
                               ? QVector3D(0.72F, 0.84F, 1.0F)
                               : accent * 0.55F + QVector3D(0.45F, 0.35F, 0.20F);
      strike_light.radius = 1.8F + 0.5F * entry.intensity;
      strike_light.intensity = 0.35F * fade;
      renderer->local_light(strike_light);
    }
  }

  auto burning_units =
      world->collect_entities_with<Engine::Core::BurningStatusComponent>();
  for (auto* burning_entity : burning_units) {
    if (burning_entity == nullptr ||
        burning_entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = burning_entity->get_component<Engine::Core::TransformComponent>();
    auto* burning =
        burning_entity->get_component<Engine::Core::BurningStatusComponent>();
    auto* unit_comp = burning_entity->get_component<Engine::Core::UnitComponent>();
    if (transform == nullptr || burning == nullptr || unit_comp == nullptr ||
        burning->remaining_duration <= 0.0F || unit_comp->health <= 0) {
      continue;
    }

    if (burning_entity->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    float const duration = std::max(0.05F, burning->duration);
    float const life_ratio =
        std::clamp(burning->remaining_duration / duration, 0.0F, 1.0F);
    float const fade_in = std::clamp(burning->ignition_elapsed / 0.18F, 0.0F, 1.0F);
    float const fade_out = std::clamp(life_ratio / 0.22F, 0.0F, 1.0F);
    float const size_scale = std::max(
        k_burning_min_scale,
        std::max({transform->scale.x, transform->scale.y, transform->scale.z}));
    int const individuals_per_unit =
        std::max(1, resolved_individuals_per_unit(*unit_comp));
    auto const surviving_individuals =
        static_cast<int>(Game::Systems::FormationCombat::living_slot_indices(
                             *burning_entity, individuals_per_unit)
                             .size());
    float const coverage_ratio = std::clamp(
        surviving_individuals / static_cast<float>(individuals_per_unit), 0.0F, 1.0F);
    float const footprint_scale =
        0.42F + 0.58F * std::sqrt(std::max(coverage_ratio, 0.0F));
    float const pulse =
        0.90F + 0.10F * std::sin(animation_time * 9.0F + transform->position.x * 0.31F +
                                 transform->position.z * 0.27F);
    float const intensity = k_burning_flame_intensity * fade_in * fade_out * pulse;
    if (intensity <= 0.01F) {
      continue;
    }

    UnitFlameAnchor const anchors[] = {
        {0.0F,
         k_burning_body_height,
         k_burning_front_offset,
         k_burning_body_radius,
         1.0F,
         0.0F,
         QVector3D(1.0F, 0.34F, 0.055F)},
        {0.08F,
         k_burning_lower_height,
         -k_burning_front_offset * 0.70F,
         k_burning_lower_radius,
         0.72F,
         0.11F,
         QVector3D(0.92F, 0.24F, 0.035F)},
        {-k_burning_side_offset,
         k_burning_body_height * 1.05F,
         0.03F,
         k_burning_shoulder_radius,
         0.66F,
         0.23F,
         QVector3D(1.0F, 0.38F, 0.07F)},
        {k_burning_side_offset,
         k_burning_body_height * 0.91F,
         -0.03F,
         k_burning_shoulder_radius * 0.88F,
         0.58F,
         0.37F,
         QVector3D(0.94F, 0.29F, 0.045F)},
    };

    constexpr std::size_t k_anchor_order[] = {0U, 1U, 2U, 3U};
    int const active_anchor_count =
        std::clamp(static_cast<int>(std::ceil(
                       coverage_ratio * static_cast<float>(std::size(k_anchor_order)))),
                   1,
                   static_cast<int>(std::size(k_anchor_order)));

    for (int anchor_idx = 0; anchor_idx < active_anchor_count; ++anchor_idx) {
      UnitFlameAnchor anchor = anchors[k_anchor_order[anchor_idx]];
      anchor.local_x *= footprint_scale;
      anchor.local_z *= footprint_scale;
      anchor.radius *= 0.68F + 0.32F * coverage_ratio;
      renderer->burning_flame(transform_unit_anchor(*transform, anchor, size_scale),
                              anchor.color,
                              anchor.radius * size_scale,
                              intensity * anchor.intensity,
                              animation_time + anchor.time_offset);
    }
  }

  auto fire_patches = world->collect_entities_with<Engine::Core::FirePatchComponent>();
  for (auto* fire_patch_entity : fire_patches) {
    if (fire_patch_entity == nullptr ||
        fire_patch_entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform =
        fire_patch_entity->get_component<Engine::Core::TransformComponent>();
    auto* fire_patch =
        fire_patch_entity->get_component<Engine::Core::FirePatchComponent>();
    if (transform == nullptr || fire_patch == nullptr ||
        fire_patch->remaining_duration <= 0.0F) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, fire_patch->radius)) {
      continue;
    }

    float const life_ratio =
        (fire_patch->duration > 0.0F)
            ? std::clamp(
                  fire_patch->remaining_duration / fire_patch->duration, 0.0F, 1.0F)
            : 0.0F;
    float const fade_in = std::clamp(
        (fire_patch->duration - fire_patch->remaining_duration) / 0.25F, 0.0F, 1.0F);
    float const fade_out = std::clamp(life_ratio / 0.35F, 0.0F, 1.0F);
    float const pulse =
        0.92F + 0.12F * std::sin(animation_time * 8.0F + transform->position.x * 0.35F +
                                 transform->position.z * 0.28F);
    float const intensity = 1.2F * fade_in * fade_out * pulse;
    if (intensity <= 0.01F) {
      continue;
    }

    QVector3D const color(k_flame_color_r, k_flame_color_g, k_flame_color_b);
    float const offset = fire_patch->radius * k_fire_patch_flame_offset_scale;
    float const base_radius =
        std::max(k_fire_patch_flame_min_radius,
                 fire_patch->radius * k_fire_patch_flame_radius_scale);
    GroundFlameLobe const lobes[] = {
        {-0.65F, 0.00F, 1.00F, 0.86F, 0.00F},
        {0.55F, 0.12F, 0.92F, 0.80F, 0.11F},
        {-0.08F, 0.58F, 0.84F, 0.74F, 0.23F},
        {0.12F, -0.56F, 0.78F, 0.70F, 0.37F},
    };

    for (GroundFlameLobe const& lobe : lobes) {
      QVector3D const position(transform->position.x + lobe.local_x * offset,
                               transform->position.y + k_fire_patch_flame_y_offset,
                               transform->position.z + lobe.local_z * offset);
      float const radius =
          base_radius * lobe.radius_scale *
          (0.96F + 0.05F * std::sin(animation_time * 6.0F + lobe.time_offset * 9.0F));

      renderer->burning_flame(position,
                              color,
                              radius,
                              intensity * lobe.intensity_scale,
                              animation_time + lobe.time_offset);
    }
  }

  auto& impact_tracker = StoneImpactTracker::instance();

  auto elephants =
      world->collect_entities_with<Engine::Core::ElephantStompImpactComponent>();
  for (auto* elephant : elephants) {
    auto* stomp_impact =
        elephant->get_component<Engine::Core::ElephantStompImpactComponent>();
    auto* transform = elephant->get_component<Engine::Core::TransformComponent>();

    if (stomp_impact == nullptr || transform == nullptr) {
      continue;
    }

    for (auto& impact : stomp_impact->impacts) {
      if (impact.time < 0.0F) {
        continue;
      }

      QVector3D const position(
          impact.x, transform->position.y + k_stone_impact_y_offset, impact.z);

      if (!is_fog_visible(position.x(), position.z())) {
        impact.time = -1.0F;
        continue;
      }

      if (!visibility.is_entity_visible(
              position.x(), position.z(), k_visibility_check_radius * 2.0F)) {
        impact.time = -1.0F;
        continue;
      }

      impact_tracker.add_impact(position,
                                animation_time,
                                k_elephant_stomp_impact_radius,
                                k_stone_impact_intensity);
      impact.time = -1.0F;
    }

    stomp_impact->impacts.erase(
        std::remove_if(
            stomp_impact->impacts.begin(),
            stomp_impact->impacts.end(),
            [](const Engine::Core::ElephantStompImpactComponent::ImpactRecord& impact) {
              return impact.time < 0.0F;
            }),
        stomp_impact->impacts.end());
  }

  impact_tracker.update(animation_time);

  for (const auto& impact : impact_tracker.impacts()) {
    if (!is_fog_visible(impact.position.x(), impact.position.z())) {
      continue;
    }

    if (!visibility.is_entity_visible(
            impact.position.x(), impact.position.z(), impact.radius)) {
      continue;
    }

    float const impact_time = animation_time - impact.start_time;

    renderer->stone_impact(
        impact.position, impact.color, impact.radius, impact.intensity, impact_time);
  }
}

} // namespace Render::GL

namespace {

using Engine::Core::CombatAnimationState;

constexpr float k_telegraph_range = 12.0F;
constexpr float k_ring_y_offset = 0.025F;
constexpr float k_marker_thickness_ratio = 0.13F;
constexpr float k_marker_min_thickness = 0.05F;

constexpr QVector3D k_danger_red{1.0F, 0.08F, 0.02F};
constexpr QVector3D k_warning_orange{1.0F, 0.55F, 0.08F};

constexpr QVector3D k_stagger_violet{0.72F, 0.42F, 1.0F};
constexpr QVector3D k_flash_white{1.0F, 0.85F, 0.45F};
constexpr QVector3D k_lock_gold{1.0F, 0.82F, 0.10F};
constexpr QVector3D k_aim_cyan{0.16F, 0.92F, 1.0F};
constexpr QVector3D k_contact_red{1.0F, 0.18F, 0.08F};

inline float pulse(float t, float hz, float phase = 0.0F) {
  return 0.5F + 0.5F * std::sin(t * 6.2832F * hz + phase);
}

enum class RpgMarkerRole : std::uint8_t {
  Stagger,
  Aim,
  Lock,
  Telegraph,
  StrikeFlash,
  ContactHit,
};

struct RpgMarkerStyle {
  Game::Accessibility::TeamPattern pattern;
  bool focused;
};

inline auto marker_style(RpgMarkerRole role) -> RpgMarkerStyle {
  switch (role) {
  case RpgMarkerRole::Stagger:
    return {Game::Accessibility::TeamPattern::Dotted, false};
  case RpgMarkerRole::Aim:
    return {Game::Accessibility::TeamPattern::Dashed, false};
  case RpgMarkerRole::Lock:
    return {Game::Accessibility::TeamPattern::DoubleRing, true};
  case RpgMarkerRole::Telegraph:
    return {Game::Accessibility::TeamPattern::Chevron, false};
  case RpgMarkerRole::StrikeFlash:
    return {Game::Accessibility::TeamPattern::Solid, false};
  case RpgMarkerRole::ContactHit:
    return {Game::Accessibility::TeamPattern::Notched, false};
  }
  return {Game::Accessibility::TeamPattern::Solid, false};
}

inline void submit_marker(Render::GL::Renderer* renderer,
                          RpgMarkerRole role,
                          float px,
                          float py,
                          float pz,
                          float radius,
                          float alpha,
                          const QVector3D& color) {
  const auto style = marker_style(role);

  Render::GL::GroundMarkerCmd marker;
  marker.center = QVector3D(px, py + k_ring_y_offset, pz);
  marker.outer_radius = radius;
  marker.thickness =
      std::max(k_marker_min_thickness, radius * k_marker_thickness_ratio);
  marker.color = color;
  marker.alpha = alpha;
  marker.pattern = style.pattern;
  marker.focused = style.focused;
  renderer->ground_marker(marker);
}

enum class BodyRingPriority : std::uint8_t {
  Stagger = 0,
  Aim = 1,
  Lock = 2,
  Telegraph = 3,
};

struct BodyRing {
  Engine::Core::EntityID entity_id{0};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  BodyRingPriority priority{BodyRingPriority::Stagger};
  RpgMarkerRole role{RpgMarkerRole::Stagger};
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float radius{0.5F};
  float alpha{0.0F};
  QVector3D color;
};

class BodyRingSet {
public:
  void add(BodyRing ring) {
    for (auto& existing : m_rings) {
      if (existing.entity_id != ring.entity_id ||
          existing.soldier_slot != ring.soldier_slot) {
        continue;
      }
      if (ring.priority > existing.priority) {
        existing = ring;
      }
      return;
    }
    m_rings.push_back(ring);
  }

  void submit(Render::GL::Renderer* renderer) const {
    for (auto const& ring : m_rings) {
      submit_marker(renderer,
                    ring.role,
                    ring.x,
                    ring.y,
                    ring.z,
                    ring.radius,
                    ring.alpha,
                    ring.color);
    }
  }

private:
  std::vector<BodyRing> m_rings;
};

} // namespace

namespace Render::GL {

void RpgTelegraphRenderer::clear() {
  m_cache.clear();
  m_strike_flashes.clear();
}

void RpgTelegraphRenderer::render(Renderer* renderer,
                                  Engine::Core::World* world,
                                  Engine::Core::EntityID commander_id,
                                  Engine::Core::EntityID locked_target_id,
                                  float anim_time) {
  using Engine::Core::CombatStateComponent;
  using Engine::Core::StaggerComponent;
  using Engine::Core::TransformComponent;

  auto* commander_ent = world->get_entity(commander_id);
  if (commander_ent == nullptr) {
    return;
  }
  auto* cmd_tf = commander_ent->get_component<TransformComponent>();
  if (cmd_tf == nullptr) {
    return;
  }
  const float cx = cmd_tf->position.x;
  const float cz = cmd_tf->position.z;

  std::vector<Engine::Core::EntityID> seen;
  seen.reserve(16);

  for (auto* entity : world->collect_entities_with<CombatStateComponent>()) {
    const auto id = entity->get_id();
    if (id == commander_id) {
      continue;
    }
    auto* csc = entity->get_component<CombatStateComponent>();
    auto* tf = entity->get_component<TransformComponent>();
    if (csc == nullptr || tf == nullptr) {
      continue;
    }
    auto const visible_attacker =
        Game::Systems::RpgCombat::resolve_damage_carrier(*entity, commander_id);
    const float ex =
        visible_attacker.has_value() ? visible_attacker->position.x() : tf->position.x;
    const float ez =
        visible_attacker.has_value() ? visible_attacker->position.z() : tf->position.z;
    const float ey =
        visible_attacker.has_value() ? visible_attacker->position.y() : tf->position.y;
    const float dx = ex - cx;
    const float dz = ez - cz;
    if (dx * dx + dz * dz > k_telegraph_range * k_telegraph_range) {
      continue;
    }

    const auto cur_state = csc->animation_state;
    auto it = m_cache.find(id);

    std::uint16_t const carrier_slot =
        visible_attacker.has_value()
            ? visible_attacker->soldier_slot
            : Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;

    if (cur_state == CombatAnimationState::WindUp) {
      seen.push_back(id);
      if (it == m_cache.end()) {
        m_cache[id] =
            TelegraphEntry{ex, ez, ey, carrier_slot, CombatAnimationState::WindUp};
      } else {

        constexpr float k_pos_epsilon = 0.01F;
        if (std::abs(it->second.last_pos_x - ex) > k_pos_epsilon ||
            std::abs(it->second.last_pos_z - ez) > k_pos_epsilon) {
          it->second.last_pos_x = ex;
          it->second.last_pos_z = ez;
          it->second.base_y = ey;
        }
        it->second.soldier_slot = carrier_slot;
        it->second.prev_state = CombatAnimationState::WindUp;
      }
    } else if (cur_state == CombatAnimationState::Strike && it != m_cache.end() &&
               it->second.prev_state == CombatAnimationState::WindUp) {

      m_strike_flashes.push_back(StrikeFlash{{ex, ey, ez}, anim_time});
      m_cache.erase(it);
    }
  }

  for (auto it = m_cache.begin(); it != m_cache.end();) {
    const bool still_seen =
        std::find(seen.begin(), seen.end(), it->first) != seen.end();
    if (!still_seen) {
      it = m_cache.erase(it);
    } else {
      ++it;
    }
  }

  m_strike_flashes.erase(std::remove_if(m_strike_flashes.begin(),
                                        m_strike_flashes.end(),
                                        [anim_time](const StrikeFlash& f) {
                                          return (anim_time - f.start_time) >=
                                                 StrikeFlash::k_duration;
                                        }),
                         m_strike_flashes.end());

  BodyRingSet body_rings;

  for (const auto& [id, entry] : m_cache) {
    auto* entity = world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* csc = entity->get_component<CombatStateComponent>();
    if (csc == nullptr) {
      continue;
    }
    const float progress =
        (csc->state_duration > 0.0F)
            ? std::clamp(csc->state_time / csc->state_duration, 0.0F, 1.0F)
            : 0.0F;

    bool unblockable = false;
    if (auto const* action =
            entity->get_component<Engine::Core::RpgCommanderActionComponent>()) {
      auto const* definition =
          Game::Systems::CombatActions::find_combat_action_definition(
              static_cast<Game::Systems::CombatActions::CombatActionId>(
                  action->combat_action_id));
      unblockable = definition != nullptr && definition->damage.unblockable;
    }

    const float ring_r =
        (unblockable ? 0.74F : 0.50F) + (unblockable ? 0.30F : 0.18F) * progress;
    const float pulse_speed =
        (unblockable ? 7.0F : 4.0F) + (unblockable ? 9.0F : 6.0F) * progress;
    const float ring_alpha =
        (unblockable ? 0.78F : 0.62F) +
        (unblockable ? 0.22F : 0.25F) * pulse(anim_time, pulse_speed);
    QVector3D const ring_color =
        unblockable ? k_danger_red
                    : k_warning_orange * (1.0F - progress) + k_danger_red * progress;

    body_rings.add({.entity_id = id,
                    .soldier_slot = entry.soldier_slot,
                    .priority = BodyRingPriority::Telegraph,
                    .role = RpgMarkerRole::Telegraph,
                    .x = entry.last_pos_x,
                    .y = entry.base_y,
                    .z = entry.last_pos_z,
                    .radius = ring_r,
                    .alpha = ring_alpha,
                    .color = ring_color});
  }

  for (auto* entity : world->collect_entities_with<StaggerComponent>()) {
    if (entity->get_id() == commander_id) {
      continue;
    }
    auto* tf = entity->get_component<TransformComponent>();
    auto* sc = entity->get_component<StaggerComponent>();
    if (tf == nullptr || sc == nullptr) {
      continue;
    }

    auto const carrier =
        Game::Systems::RpgCombat::resolve_damage_carrier(*entity, commander_id);
    const float ex = carrier.has_value() ? carrier->position.x() : tf->position.x;
    const float ey = carrier.has_value() ? carrier->position.y() : tf->position.y;
    const float ez = carrier.has_value() ? carrier->position.z() : tf->position.z;
    const float dx = ex - cx;
    const float dz = ez - cz;
    if (dx * dx + dz * dz > k_telegraph_range * k_telegraph_range) {
      continue;
    }

    const float fade = std::clamp(sc->remaining / 0.5F, 0.0F, 1.0F);
    const float stagger_alpha = 0.70F * fade * (0.7F + 0.3F * pulse(anim_time, 6.0F));
    body_rings.add(
        {.entity_id = entity->get_id(),
         .soldier_slot =
             carrier.has_value()
                 ? carrier->soldier_slot
                 : Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot,
         .priority = BodyRingPriority::Stagger,
         .role = RpgMarkerRole::Stagger,
         .x = ex,
         .y = ey,
         .z = ez,
         .radius = carrier.has_value() ? std::max(0.58F, carrier->body_radius * 1.24F)
                                       : 0.90F,
         .alpha = stagger_alpha,
         .color = k_stagger_violet});
  }

  for (const auto& flash : m_strike_flashes) {
    const float elapsed = anim_time - flash.start_time;
    const float t = elapsed / StrikeFlash::k_duration;

    const float flash_r = 0.30F + 0.26F * t;
    const float flash_alpha = (1.0F - t) * (1.0F - t) * 0.62F;
    submit_marker(renderer,
                  RpgMarkerRole::StrikeFlash,
                  flash.pos.x(),
                  flash.pos.y(),
                  flash.pos.z(),
                  flash_r,
                  flash_alpha,
                  k_flash_white);

    if (elapsed < 0.08F) {
      QVector3D const spark_pos(flash.pos.x(), flash.pos.y() + 0.4F, flash.pos.z());
      QVector3D const spark_color(1.0F, 0.85F, 0.4F);
      renderer->metal_spark(spark_pos, spark_color, 0.24F, 1.15F, elapsed);
    }
  }

  auto const* targets =
      commander_ent->get_component<Engine::Core::RpgCommanderTargetComponent>();
  auto resolve_target = [world](Engine::Core::EntityID entity_id,
                                std::uint16_t soldier_slot)
      -> std::optional<Game::Systems::RpgCombat::SoldierTarget> {
    auto* entity = world->get_entity(entity_id);
    return entity != nullptr
               ? Game::Systems::RpgCombat::resolve_soldier_target(*entity, soldier_slot)
               : std::nullopt;
  };

  Engine::Core::EntityID const resolved_lock_id =
      targets != nullptr ? targets->explicit_lock_target_id : locked_target_id;
  std::uint16_t const resolved_lock_slot =
      targets != nullptr ? targets->explicit_lock_soldier_slot
                         : Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  bool aim_ring_active = false;
  if (targets != nullptr && targets->aim_candidate_in_range &&
      targets->aim_candidate_id != 0) {
    auto const aim =
        resolve_target(targets->aim_candidate_id, targets->aim_candidate_soldier_slot);
    if (aim.has_value()) {
      bool const aim_is_lock =
          targets->aim_candidate_id == resolved_lock_id &&
          targets->aim_candidate_soldier_slot == resolved_lock_slot;
      aim_ring_active = true;
      float const alpha = 0.78F + 0.18F * pulse(anim_time, 4.0F);
      float const radius =
          std::max(0.54F, aim->body_radius * (aim_is_lock ? 1.30F : 1.22F));
      body_rings.add(
          {.entity_id = targets->aim_candidate_id,
           .soldier_slot = targets->aim_candidate_soldier_slot,
           .priority = aim_is_lock ? BodyRingPriority::Lock : BodyRingPriority::Aim,
           .role = aim_is_lock ? RpgMarkerRole::Lock : RpgMarkerRole::Aim,
           .x = aim->position.x(),
           .y = aim->position.y(),
           .z = aim->position.z(),
           .radius = radius,
           .alpha = alpha,
           .color = aim_is_lock ? k_lock_gold : k_aim_cyan});
    }
  }

  if (resolved_lock_id != 0 && !aim_ring_active) {
    auto const lock = resolve_target(resolved_lock_id, resolved_lock_slot);
    if (lock.has_value()) {
      float const lock_alpha = 0.72F + 0.22F * pulse(anim_time, 3.0F);
      float const radius = std::max(0.62F, lock->body_radius * 1.36F);
      body_rings.add({.entity_id = resolved_lock_id,
                      .soldier_slot = resolved_lock_slot,
                      .priority = BodyRingPriority::Lock,
                      .role = RpgMarkerRole::Lock,
                      .x = lock->position.x(),
                      .y = lock->position.y(),
                      .z = lock->position.z(),
                      .radius = radius,
                      .alpha = lock_alpha,
                      .color = k_lock_gold});
    }
  }

  body_rings.submit(renderer);

  if (targets != nullptr && targets->recent_hit_target_id != 0 &&
      targets->recent_hit_timer > 0.0F) {
    auto const hit =
        resolve_target(targets->recent_hit_target_id, targets->recent_hit_soldier_slot);
    if (hit.has_value()) {
      float const life = std::clamp(targets->recent_hit_timer / 0.28F, 0.0F, 1.0F);
      float const radius = std::max(0.50F, hit->body_radius) + (1.0F - life) * 0.55F;
      submit_marker(renderer,
                    RpgMarkerRole::ContactHit,
                    hit->position.x(),
                    hit->position.y(),
                    hit->position.z(),
                    radius,
                    life * 0.72F,
                    k_contact_red);
    }
  }
}

} // namespace Render::GL
