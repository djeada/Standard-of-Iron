#include "wading_effects_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

#include "formation_instance_layout.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/units/spawn_type.h"
#include "render/camera_visibility.h"
#include "render/draw_commands.h"
#include "render/scene_renderer.h"

namespace Render::GL {

namespace {

// Rings hug the water only where it is deep enough for the marker to float
// rather than drape onto the bed (ground_marker.vert drapes within 0.25).
constexpr float k_min_ring_depth = 0.26F;
constexpr float k_ring_lift = 0.05F;
constexpr float k_ripple_period = 1.4F;
constexpr float k_moving_speed = 0.15F;
constexpr float k_spray_speed = 0.45F;
constexpr int k_wake_rings = 3;
constexpr int k_max_markers = 1200;
constexpr int k_max_sprays = 80;
constexpr float k_visibility_radius = 6.0F;

const QVector3D k_foam_color{0.80F, 0.89F, 0.92F};
const QVector3D k_spray_color{0.86F, 0.93F, 0.97F};

auto hash01(std::uint64_t value) -> float {
  value ^= value >> 33U;
  value *= 0xff51afd7ed558ccdULL;
  value ^= value >> 33U;
  value *= 0xc4ceb9fe1a85ec53ULL;
  value ^= value >> 33U;
  return static_cast<float>(value & 0xFFFFFFU) / static_cast<float>(0xFFFFFFU);
}

struct BodySize {
  float ring;
  float wake_step;
};

auto body_size_for(Engine::Core::World& world,
                   Engine::Core::EntityID id,
                   const Engine::Core::UnitComponent* unit) -> BodySize {
  if (world.has<Engine::Core::ElephantComponent>(id)) {
    return {0.95F, 0.9F};
  }
  if (unit != nullptr && Game::Units::is_cavalry(unit->spawn_type)) {
    return {0.42F, 0.55F};
  }
  return {0.21F, 0.30F};
}

} // namespace

void render_wading_effects(Renderer* renderer,
                           ResourceManager*,
                           Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }
  auto const& world_view = renderer->world_view();
  auto const* terrain = world_view.terrain();
  if (terrain == nullptr || !terrain->is_initialized() || !terrain->has_fords()) {
    return;
  }

  float const time = renderer->get_animation_time();
  auto& camera = CameraVisibility::instance();
  auto fog_snapshot =
      world_view.has_visibility() ? world_view.visibility()->snapshot_ptr() : nullptr;

  int markers = 0;
  int sprays = 0;
  for (auto [id, wading, transform] :
       world->view<Engine::Core::WadingComponent, Engine::Core::TransformComponent>()) {
    (void)wading;
    if (markers >= k_max_markers) {
      break;
    }
    if (world->has<Engine::Core::PendingRemovalComponent>(id)) {
      continue;
    }
    auto const* unit = world->try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    float const cx = transform.position.x;
    float const cz = transform.position.z;
    if (fog_snapshot != nullptr &&
        !Game::Map::should_render_combat_effect(*fog_snapshot, cx, cz)) {
      continue;
    }
    if (!camera.is_entity_visible(cx, cz, k_visibility_radius)) {
      continue;
    }

    BodySize const body = body_size_for(*world, id, unit);

    std::span<const Engine::Core::FormationSoldierPresentation> soldiers;
    if (auto const* formation =
            world->try_get<Engine::Core::FormationPresentationComponent>(id)) {
      soldiers = formation->soldiers;
    }
    auto const* entity = world->get_entity(id);
    auto const root = Render::Entity::resolve_formation_root(entity, transform);
    auto const frame = Render::Entity::formation_world_frame(root.position, root.yaw);

    QVector3D unit_velocity;
    if (auto const* movement = world->try_get<Engine::Core::MovementComponent>(id)) {
      unit_velocity = QVector3D(movement->get_vx(), 0.0F, movement->get_vz());
    }

    auto const stir_water = [&](float x, float z, float vx, float vz, std::uint64_t seed) {
      if (markers >= k_max_markers) {
        return;
      }
      float const depth = terrain->ford_water_depth_at(x, z);
      if (depth < k_min_ring_depth) {
        return;
      }
      auto const level = terrain->ford_water_level_at(x, z);
      if (!level.has_value()) {
        return;
      }
      float const y = *level + k_ring_lift;
      float const speed = std::hypot(vx, vz);
      float const stir = std::clamp(speed / 1.2F, 0.0F, 1.0F);

      GroundMarkerCmd waterline;
      waterline.center = QVector3D(x, y, z);
      waterline.outer_radius = body.ring * (1.0F + 0.15F * stir);
      waterline.thickness = body.ring * 0.32F;
      waterline.color = k_foam_color;
      waterline.alpha = 0.42F + 0.25F * stir;
      waterline.priority = CommandPriority::Normal;
      renderer->ground_marker(waterline);
      ++markers;

      float const phase = std::fmod((time / k_ripple_period) + hash01(seed), 1.0F);
      GroundMarkerCmd ripple = waterline;
      ripple.outer_radius = body.ring * (1.15F + 1.9F * phase);
      ripple.thickness = body.ring * 0.18F;
      ripple.alpha = 0.30F * (1.0F - phase) * (1.0F - phase);
      renderer->ground_marker(ripple);
      ++markers;

      if (speed < k_moving_speed) {
        return;
      }
      float const dir_x = vx / speed;
      float const dir_z = vz / speed;

      GroundMarkerCmd bow = waterline;
      bow.center =
          QVector3D(x + dir_x * body.ring * 0.9F, y, z + dir_z * body.ring * 0.9F);
      bow.outer_radius = body.ring * 0.75F;
      bow.thickness = body.ring * 0.30F;
      bow.alpha = 0.40F * stir + 0.15F;
      renderer->ground_marker(bow);
      ++markers;

      for (int ring = 1; ring <= k_wake_rings; ++ring) {
        float const back =
            body.wake_step * static_cast<float>(ring) * (0.8F + 0.6F * stir);
        GroundMarkerCmd wake = waterline;
        wake.center = QVector3D(x - dir_x * back, y, z - dir_z * back);
        wake.outer_radius = body.ring * (1.1F + 0.55F * static_cast<float>(ring));
        wake.thickness = body.ring * 0.16F;
        wake.alpha = (0.34F - 0.08F * static_cast<float>(ring)) * (0.4F + 0.6F * stir);
        renderer->ground_marker(wake);
        ++markers;
      }

      if (speed >= k_spray_speed && sprays < k_max_sprays) {
        float const bucket = std::floor(time * 6.0F);
        if (hash01(seed ^ static_cast<std::uint64_t>(bucket)) < 0.35F * stir + 0.1F) {
          renderer->metal_spark(
              QVector3D(x + dir_x * body.ring, y + 0.04F, z + dir_z * body.ring),
              k_spray_color,
              body.ring * 1.3F,
              0.55F + 0.35F * stir,
              time,
              QVector3D(dir_x * 0.4F, 1.0F, dir_z * 0.4F).normalized());
          ++sprays;
        }
      }
    };

    if (soldiers.empty()) {
      stir_water(cx, cz, unit_velocity.x(), unit_velocity.z(), id * 131U);
      continue;
    }
    for (std::size_t index = 0; index < soldiers.size(); ++index) {
      auto const& soldier = soldiers[index];
      if (!soldier.alive) {
        continue;
      }
      float x = 0.0F;
      float z = 0.0F;
      float vx = unit_velocity.x();
      float vz = unit_velocity.z();
      if (soldier.world_motion_valid) {
        x = soldier.world_x;
        z = soldier.world_z;
        vx = soldier.world_velocity_x;
        vz = soldier.world_velocity_z;
      } else {
        auto const anchor =
            frame.map(QVector3D(soldier.local_x, 0.0F, soldier.local_z));
        x = anchor.x();
        z = anchor.z();
      }
      stir_water(x, z, vx, vz, (id * 131U) + index);
    }
  }
}

} // namespace Render::GL
