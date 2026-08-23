#include "structure_combat.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>

#include "../../core/component.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"
#include "../formation_combat_geometry.h"
#include "../nav_grid.h"
#include "../pathfinding.h"

namespace Game::Systems::Combat {
namespace {

struct StructureFootprint {
  float center_x{0.0F};
  float center_z{0.0F};
  float half_width{1.0F};
  float half_depth{1.0F};
  float yaw_radians{0.0F};
};

[[nodiscard]] auto
structure_footprint(const Engine::Core::Entity& structure) -> StructureFootprint {
  StructureFootprint result;
  auto const* transform = structure.get_component<Engine::Core::TransformComponent>();
  auto const* unit = structure.get_component<Engine::Core::UnitComponent>();
  if (transform != nullptr) {
    result.center_x = transform->position.x;
    result.center_z = transform->position.z;
    result.yaw_radians = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  }

  auto size = Game::Systems::BuildingCollisionRegistry::get_building_size(
      unit != nullptr ? Game::Units::spawn_typeToString(unit->spawn_type)
                      : std::string{});
  for (auto const& registered :
       Game::Systems::BuildingCollisionRegistry::instance().get_all_buildings()) {
    if (registered.entity_id != structure.get_id()) {
      continue;
    }
    size.width = registered.width;
    size.depth = registered.depth;
    break;
  }
  result.half_width = std::max(0.1F, size.width * 0.5F);
  result.half_depth = std::max(0.1F, size.depth * 0.5F);
  return result;
}

[[nodiscard]] auto world_to_local(const StructureFootprint& footprint,
                                  float world_x,
                                  float world_z) -> std::pair<float, float> {
  float const dx = world_x - footprint.center_x;
  float const dz = world_z - footprint.center_z;
  float const sin_yaw = std::sin(footprint.yaw_radians);
  float const cos_yaw = std::cos(footprint.yaw_radians);
  return {cos_yaw * dx - sin_yaw * dz, sin_yaw * dx + cos_yaw * dz};
}

[[nodiscard]] auto local_to_world(const StructureFootprint& footprint,
                                  float local_x,
                                  float local_z) -> QVector3D {
  float const sin_yaw = std::sin(footprint.yaw_radians);
  float const cos_yaw = std::cos(footprint.yaw_radians);
  return {footprint.center_x + cos_yaw * local_x + sin_yaw * local_z,
          0.0F,
          footprint.center_z - sin_yaw * local_x + cos_yaw * local_z};
}

[[nodiscard]] auto rotate_local_vector(const StructureFootprint& footprint,
                                       float local_x,
                                       float local_z) -> QVector3D {
  float const sin_yaw = std::sin(footprint.yaw_radians);
  float const cos_yaw = std::cos(footprint.yaw_radians);
  return {cos_yaw * local_x + sin_yaw * local_z,
          0.0F,
          -sin_yaw * local_x + cos_yaw * local_z};
}

[[nodiscard]] auto structure_height(const Engine::Core::Entity& structure) -> float {
  auto const* unit = structure.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return 0.9F;
  }
  using Game::Units::SpawnType;
  switch (unit->spawn_type) {
  case SpawnType::DefenseTower:
    return 1.65F;
  case SpawnType::Barracks:
    return 1.25F;
  case SpawnType::Marketplace:
    return 1.15F;
  case SpawnType::Temple:
    return 1.45F;
  case SpawnType::Farm:
    return 0.7F;
  case SpawnType::Home:
    return 1.05F;
  case SpawnType::WallSegment:
    return 0.82F;
  default:
    return 0.9F;
  }
}

[[nodiscard]] auto closest_attacker_anchor(const Engine::Core::Entity& attacker,
                                           const Engine::Core::Entity& structure)
    -> std::pair<QVector3D, StructureSurfaceContact> {
  auto const layout = Game::Systems::FormationCombat::resolve_layout(attacker);
  auto const* transform = attacker.get_component<Engine::Core::TransformComponent>();
  QVector3D fallback;
  if (transform != nullptr) {
    fallback = {transform->position.x, transform->position.y, transform->position.z};
  }

  QVector3D best_anchor = fallback;
  auto best_surface = closest_structure_surface(structure, fallback);
  float best_distance = best_surface.distance;
  for (auto const& slot : layout.live_slots) {
    QVector3D const anchor(slot.world_x, fallback.y(), slot.world_z);
    auto const surface = closest_structure_surface(structure, anchor);
    if (surface.distance < best_distance) {
      best_distance = surface.distance;
      best_anchor = anchor;
      best_surface = surface;
    }
  }
  return {best_anchor, best_surface};
}

} // namespace

auto structure_attack_profile(const Engine::Core::Entity* attacker)
    -> StructureAttackProfile {
  StructureAttackProfile profile;
  if (attacker == nullptr) {
    profile.damage_multiplier = 1.0F;
    profile.minimum_damage = 0;
    profile.impact_style = StructureImpactStyle::HeavyMelee;
    return profile;
  }

  auto const* unit = attacker->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return profile;
  }

  using Game::Units::SpawnType;
  switch (unit->spawn_type) {
  case SpawnType::Archer:
  case SpawnType::HorseArcher:
  case SpawnType::SkeletonArcher:
  case SpawnType::DefenseTower:
    profile.damage_multiplier = 0.0F;
    profile.minimum_damage = 0;
    break;
  case SpawnType::Catapult:
    profile.damage_multiplier = 1.75F;
    profile.minimum_damage = 1;
    profile.contact_clearance = 1.0F;
    profile.impact_height = 1.2F;
    profile.impact_style = StructureImpactStyle::Catapult;
    break;
  case SpawnType::Ballista:
    profile.damage_multiplier = 1.25F;
    profile.minimum_damage = 1;
    profile.contact_clearance = 1.0F;
    profile.impact_height = 1.05F;
    profile.impact_style = StructureImpactStyle::Ballista;
    break;
  case SpawnType::Elephant:
    profile.damage_multiplier = 1.60F;
    profile.minimum_damage = 1;
    profile.contact_clearance = 1.25F;
    profile.impact_height = 0.75F;
    profile.impact_style = StructureImpactStyle::Elephant;
    break;
  case SpawnType::GravePriest:
    profile.damage_multiplier = 0.35F;
    profile.minimum_damage = 1;
    profile.impact_height = 1.0F;
    profile.impact_style = StructureImpactStyle::Magic;
    break;
  case SpawnType::Spearman:
  case SpawnType::HorseSpearman:
    profile.damage_multiplier = 0.12F;
    profile.minimum_damage = 1;
    profile.contact_clearance = 0.82F;
    break;
  case SpawnType::MountedKnight:
    profile.damage_multiplier = 0.12F;
    profile.minimum_damage = 1;
    profile.contact_clearance = 0.92F;
    profile.impact_style = StructureImpactStyle::HeavyMelee;
    break;
  case SpawnType::Knight:
  case SpawnType::SkeletonSwordsman:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageSpearCommander:
  case SpawnType::CarthageBowCommander:
  case SpawnType::CarthageSwordCommander:
  case SpawnType::Healer:
  case SpawnType::Civilian:
  case SpawnType::Builder:
    profile.damage_multiplier = 0.12F;
    profile.minimum_damage = 1;
    profile.contact_clearance = 0.62F;
    break;
  case SpawnType::Barracks:
  case SpawnType::Home:
  case SpawnType::WallSegment:
  case SpawnType::WallGate:
  case SpawnType::Marketplace:
  case SpawnType::Temple:
  case SpawnType::Farm:
  case SpawnType::Sheep:
  case SpawnType::Wolf:
    profile.damage_multiplier = 0.0F;
    profile.minimum_damage = 0;
    break;
  }
  return profile;
}

auto resolve_structure_damage(const Engine::Core::Entity* attacker,
                              int raw_damage) -> int {
  if (raw_damage <= 0) {
    return 0;
  }
  auto const profile = structure_attack_profile(attacker);
  if (profile.damage_multiplier <= 0.0F) {
    return 0;
  }
  int const scaled = static_cast<int>(
      std::round(static_cast<float>(raw_damage) * profile.damage_multiplier));
  return std::max(profile.minimum_damage, scaled);
}

auto closest_structure_surface(const Engine::Core::Entity& structure,
                               const QVector3D& query_point)
    -> StructureSurfaceContact {
  auto const footprint = structure_footprint(structure);
  auto const [local_x, local_z] =
      world_to_local(footprint, query_point.x(), query_point.z());
  float surface_x = std::clamp(local_x, -footprint.half_width, footprint.half_width);
  float surface_z = std::clamp(local_z, -footprint.half_depth, footprint.half_depth);

  float normal_x = 0.0F;
  float normal_z = 0.0F;
  float tangent_half_extent = footprint.half_width;
  bool const inside = std::abs(local_x) <= footprint.half_width &&
                      std::abs(local_z) <= footprint.half_depth;
  if (inside) {
    float const x_clearance = footprint.half_width - std::abs(local_x);
    float const z_clearance = footprint.half_depth - std::abs(local_z);
    if (x_clearance < z_clearance) {
      normal_x = local_x < 0.0F ? -1.0F : 1.0F;
      surface_x = normal_x * footprint.half_width;
      tangent_half_extent = footprint.half_depth;
    } else {
      normal_z = local_z < 0.0F ? -1.0F : 1.0F;
      surface_z = normal_z * footprint.half_depth;
      tangent_half_extent = footprint.half_width;
    }
  } else {
    float const delta_x = local_x - surface_x;
    float const delta_z = local_z - surface_z;
    float const distance = std::hypot(delta_x, delta_z);
    if (distance > 0.0001F) {
      normal_x = delta_x / distance;
      normal_z = delta_z / distance;
    } else if (std::abs(local_x) > footprint.half_width) {
      normal_x = local_x < 0.0F ? -1.0F : 1.0F;
    } else {
      normal_z = local_z < 0.0F ? -1.0F : 1.0F;
    }
    tangent_half_extent = std::abs(normal_x) > std::abs(normal_z)
                              ? footprint.half_depth
                              : footprint.half_width;
  }

  QVector3D point = local_to_world(footprint, surface_x, surface_z);
  auto outward = rotate_local_vector(footprint, normal_x, normal_z);
  if (outward.lengthSquared() <= 0.000001F) {
    outward = {0.0F, 0.0F, -1.0F};
  } else {
    outward.normalize();
  }
  QVector3D const tangent(-outward.z(), 0.0F, outward.x());
  point.setY(query_point.y());
  return {.point = point,
          .outward_normal = outward,
          .tangent = tangent,
          .distance = inside ? 0.0F
                             : std::hypot(query_point.x() - point.x(),
                                          query_point.z() - point.z()),
          .tangent_half_extent = tangent_half_extent};
}

auto structure_impact_point(const Engine::Core::Entity& structure,
                            const QVector3D& source,
                            float lateral_offset,
                            float height_override) -> QVector3D {
  auto const surface = closest_structure_surface(structure, source);
  float const safe_lateral = std::clamp(lateral_offset,
                                        -surface.tangent_half_extent * 0.82F,
                                        surface.tangent_half_extent * 0.82F);
  QVector3D point = surface.point + surface.tangent * safe_lateral;
  auto const* transform = structure.get_component<Engine::Core::TransformComponent>();
  float const base_y = transform != nullptr ? transform->position.y : 0.0F;
  point.setY(base_y +
             (height_override >= 0.0F ? height_override : structure_height(structure)));
  return point;
}

auto structure_melee_approach(const Engine::Core::Entity& attacker,
                              const Engine::Core::Entity& structure)
    -> StructureApproach {
  StructureApproach result;
  auto const* transform = attacker.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    result.reached = true;
    return result;
  }
  result.destination = {
      transform->position.x, transform->position.y, transform->position.z};
  auto const [anchor, surface] = closest_attacker_anchor(attacker, structure);
  (void)anchor;
  result.current_surface_gap = surface.distance;
  result.desired_surface_gap = structure_attack_profile(&attacker).contact_clearance;
  result.reached = result.current_surface_gap <= result.desired_surface_gap + 0.05F;
  if (!result.reached) {
    float const travel = result.current_surface_gap - result.desired_surface_gap;
    result.destination -= surface.outward_normal * travel;
  }
  return result;
}

auto structure_navigation_melee_approach(const Engine::Core::Entity& attacker,
                                         const Engine::Core::Entity& structure,
                                         float extra_tolerance) -> StructureApproach {
  StructureApproach result = structure_melee_approach(attacker, structure);
  auto const* transform = attacker.get_component<Engine::Core::TransformComponent>();
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (transform == nullptr || pathfinder == nullptr) {
    return result;
  }

  pathfinder->update_navigation_grid();
  auto const* movement = attacker.get_component<Engine::Core::MovementComponent>();
  auto const passability = movement != nullptr && !movement->get_can_enter_forest()
                               ? Game::Systems::Pathfinding::Passability::Heavy
                               : Game::Systems::Pathfinding::Passability::Light;
  QVector3D const current(transform->position.x, 0.0F, transform->position.z);
  bool const current_walkable =
      pathfinder->is_world_position_walkable(current, passability, 0.0F);
  if (current_walkable && result.reached) {
    return result;
  }

  if (!pathfinder->is_world_position_walkable(result.destination, passability, 0.0F)) {
    auto const [anchor, surface] = closest_attacker_anchor(attacker, structure);
    (void)anchor;
    float const cell_size = std::max(0.1F, pathfinder->grid_cell_size());
    float const step = cell_size * 0.1F;
    float const max_outward_search =
        BuildingCollisionRegistry::get_grid_padding() + cell_size * 1.5F;
    bool resolved = false;
    for (float distance = step; distance <= max_outward_search; distance += step) {
      QVector3D const candidate =
          result.destination + surface.outward_normal * distance;
      if (!pathfinder->is_world_position_walkable(candidate, passability, 0.0F)) {
        continue;
      }
      result.destination = candidate;
      resolved = true;
      break;
    }
    if (!resolved) {
      result.reached = false;
      return result;
    }
  }

  float const root_distance = std::hypot(result.destination.x() - current.x(),
                                         result.destination.z() - current.z());
  float const arrival_tolerance = 0.08F + std::max(0.0F, extra_tolerance);
  result.reached = current_walkable && root_distance <= arrival_tolerance;
  return result;
}

auto structure_melee_contact_active(const Engine::Core::Entity& attacker,
                                    const Engine::Core::Entity& structure,
                                    float extra_tolerance) -> bool {
  return structure_navigation_melee_approach(attacker, structure, extra_tolerance)
      .reached;
}

auto structure_surface_distance(const Engine::Core::Entity& structure,
                                const QVector3D& point) -> float {
  return closest_structure_surface(structure, point).distance;
}

} // namespace Game::Systems::Combat
