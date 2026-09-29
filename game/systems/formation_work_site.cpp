#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "../core/component.h"
#include "building_collision_registry.h"
#include "formation_geometry_internal.h"

namespace Game::Systems::FormationCombat::Detail {
namespace {

constexpr float k_work_site_clearance = 0.35F;
constexpr float k_work_site_centre_tolerance = 0.5F;

} // namespace

auto work_site_for(const Engine::Core::Entity& entity,
                   const Engine::Core::TransformComponent& transform) -> WorkSite {
  auto const* builder =
      entity.get_component<Engine::Core::BuilderProductionComponent>();
  if (builder == nullptr || !builder->in_progress || !builder->at_construction_site) {
    return {};
  }
  WorkSite site;
  site.active = true;
  if (!builder->has_construction_site) {
    return site;
  }
  float const dx = transform.position.x - builder->construction_site_x;
  float const dz = transform.position.z - builder->construction_site_z;
  if ((dx * dx) + (dz * dz) >
      k_work_site_centre_tolerance * k_work_site_centre_tolerance) {
    return site;
  }

  auto const size = BuildingCollisionRegistry::get_building_size(builder->product_type);
  site.half_width = std::max(0.0F, size.width * 0.5F);
  site.half_depth = std::max(0.0F, size.depth * 0.5F);
  site.relative_yaw_radians =
      (transform.rotation.y - builder->construction_site_rotation_y) *
      std::numbers::pi_v<float> / 180.0F;
  return site;
}

void place_on_site_perimeter(const WorkSite& site, float& offset_x, float& offset_z) {
  if (site.half_width <= 0.0F || site.half_depth <= 0.0F) {
    return;
  }
  float const length = std::hypot(offset_x, offset_z);
  if (length < 1.0e-4F) {
    return;
  }
  float const sin_yaw = std::sin(site.relative_yaw_radians);
  float const cos_yaw = std::cos(site.relative_yaw_radians);
  float const unit_x = offset_x / length;
  float const unit_z = offset_z / length;
  float const site_x = cos_yaw * unit_x + sin_yaw * unit_z;
  float const site_z = -sin_yaw * unit_x + cos_yaw * unit_z;
  float const reach_x = std::abs(site_x) > 1.0e-4F ? site.half_width / std::abs(site_x)
                                                   : std::numeric_limits<float>::max();
  float const reach_z = std::abs(site_z) > 1.0e-4F ? site.half_depth / std::abs(site_z)
                                                   : std::numeric_limits<float>::max();
  float const distance =
      std::max(length, std::min(reach_x, reach_z) + k_work_site_clearance);
  offset_x = unit_x * distance;
  offset_z = unit_z * distance;
}

} // namespace Game::Systems::FormationCombat::Detail
