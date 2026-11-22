#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "render/entity/building_collapse.h"
#include "render/entity/combat_dust_renderer_internal.h"
#include "render/frame/camera_visibility.h"
#include "render/frame/combat_dust_defaults.h"
#include "render/frame/frame_submitter.h"

namespace Render::GL {

using namespace combat_dust_detail;

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

void render_blood_stains(FrameSubmitter* renderer,
                         const DrawResources*,
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

} // namespace Render::GL
