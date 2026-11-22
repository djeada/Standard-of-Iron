#include "render/entity/combat_dust_renderer.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system/structure_combat.h"
#include "game/systems/combat_system/structure_fire.h"
#include "game/systems/formation_combat_geometry.h"
#include "render/entity/building_collapse.h"
#include "render/entity/combat_dust_renderer_internal.h"
#include "render/frame/camera_visibility.h"
#include "render/frame/combat_dust_defaults.h"
#include "render/frame/frame_submitter.h"

namespace Render::GL {

using namespace combat_dust_detail;

namespace {

constexpr float k_degrees_to_radians = 0.017453292519943295F;
constexpr float k_dust_y_offset = 0.05F;
constexpr float k_dust_color_r = 0.64F;
constexpr float k_dust_color_g = 0.54F;
constexpr float k_dust_color_b = 0.39F;

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

constexpr float k_elephant_stomp_impact_radius = 0.52F;
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

struct GroundDustCluster {
  float sum_x{0.0F};
  float sum_z{0.0F};
  float weight{0.0F};
  int cell_x{0};
  int cell_z{0};
};

struct GroundDustAppearance {
  QVector3D color{k_dust_color_r, k_dust_color_g, k_dust_color_b};
  float intensity_scale{1.0F};
};

auto ground_dust_appearance(const FrameSubmitter& renderer) -> GroundDustAppearance {
  GroundDustAppearance result;
  float wetness = renderer.environment_lighting().wetness;
  const auto* terrain = renderer.world_view().terrain();
  if (terrain != nullptr) {
    const auto& biome = terrain->biome_settings();
    const auto surface = Game::Map::make_surface_profile(biome);
    const auto climate = Game::Map::make_climate_profile(biome);
    result.color = surface.soil_color * 0.72F + surface.grass_dry * 0.28F;

    result.color = result.color * 0.85F + QVector3D(0.12F, 0.11F, 0.09F);
    wetness = std::max(wetness, climate.moisture_level * 0.65F);
    result.intensity_scale *= 1.0F - std::clamp(climate.snow_coverage, 0.0F, 1.0F);
  }
  result.intensity_scale *= 1.0F - 0.88F * std::clamp(wetness, 0.0F, 1.0F);
  return result;
}

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

void emit_ground_dust(FrameSubmitter* renderer,
                      std::vector<GroundDustCluster>& clusters,
                      float animation_time) {
  namespace Defaults = Render::CombatDustDefaults;
  const auto appearance = ground_dust_appearance(*renderer);
  const auto* terrain = renderer->world_view().terrain();

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

    const float world_x = cluster.sum_x / cluster.weight;
    const float world_z = cluster.sum_z / cluster.weight;
    const float ground_y =
        terrain != nullptr ? terrain->resolve_surface_world_y(world_x, world_z) : 0.0F;
    QVector3D const position(world_x, ground_y + k_dust_y_offset, world_z);
    renderer->combat_dust(position,
                          appearance.color,
                          radius,
                          intensity * appearance.intensity_scale,
                          animation_time +
                              dust_cell_phase(cluster.cell_x, cluster.cell_z));
  }
}

void render_combat_dust(FrameSubmitter* renderer,
                        const DrawResources*,
                        Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float const animation_time = renderer->get_animation_time();
  const auto dust_appearance = ground_dust_appearance(*renderer);
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

  for (auto [entity_id, death, building] :
       world->view<Engine::Core::DeathAnimationComponent,
                   Engine::Core::BuildingComponent>()) {
    (void)death;
    (void)building;
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }
    auto const collapse = resolve_building_collapse(*world, entity_id);
    if (!collapse.active || !is_fog_visible(collapse.base.x(), collapse.base.z()) ||
        !visibility.is_entity_visible(
            collapse.base.x(),
            collapse.base.z(),
            std::max(k_visibility_check_radius,
                     collapse.footprint.half_width + collapse.footprint.half_depth))) {
      continue;
    }
    submit_building_collapse_effects(*renderer, collapse, animation_time);
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

      if (contact.outcome == Engine::Core::RpgContactOutcome::Dodge) {
        continue;
      }
      float const contact_fade =
          1.0F -
          std::clamp(contact.age / std::max(0.01F, contact.lifetime), 0.0F, 1.0F);
      if (contact_fade <= 0.0F) {
        continue;
      }
      QVector3D const grit_tint = dust_appearance.color;
      renderer->combat_dust(QVector3D(contact.x, contact.y - 0.10F, contact.z),
                            grit_tint,
                            (0.36F + 0.18F * contact.intensity) *
                                (1.0F + 0.35F * (1.0F - contact_fade)),
                            0.42F * contact.intensity * contact_fade * contact_fade *
                                dust_appearance.intensity_scale,
                            animation_time + contact.x * 0.19F + contact.z * 0.13F);
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

      if (entry.form != Engine::Core::CommanderSignatureForm::Shot) {
        const bool thrust = entry.form == Engine::Core::CommanderSignatureForm::Thrust;
        renderer->weapon_arc(contact + forward * 0.035F,
                             thrust ? QVector3D(0.52F, 0.78F, 1.0F) : accent,
                             thrust ? 0.48F : 0.62F,
                             entry.intensity * 0.65F,
                             progress,
                             across,
                             1.0F,
                             std::numbers::pi_v<float> * 0.5F);
      }

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
      strike_light.intensity = 0.55F * fade;
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
