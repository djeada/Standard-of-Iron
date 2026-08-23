#pragma once

#include <QVector3D>

#include <cstdint>

#include "../../core/entity.h"

namespace Game::Systems::Combat {

enum class StructureImpactStyle : std::uint8_t {
  LightMelee,
  HeavyMelee,
  Elephant,
  Ballista,
  Catapult,
  Magic,
};

struct StructureAttackProfile {
  float damage_multiplier{1.0F};
  int minimum_damage{0};
  float contact_clearance{0.62F};
  float impact_height{0.9F};
  StructureImpactStyle impact_style{StructureImpactStyle::LightMelee};
};

struct StructureSurfaceContact {
  QVector3D point;
  QVector3D outward_normal{0.0F, 0.0F, -1.0F};
  QVector3D tangent{1.0F, 0.0F, 0.0F};
  float distance{0.0F};
  float tangent_half_extent{1.0F};
};

struct StructureApproach {
  QVector3D destination;
  float current_surface_gap{0.0F};
  float desired_surface_gap{0.0F};
  bool reached{false};
};

[[nodiscard]] auto structure_attack_profile(const Engine::Core::Entity* attacker)
    -> StructureAttackProfile;

[[nodiscard]] auto resolve_structure_damage(const Engine::Core::Entity* attacker,
                                            int raw_damage) -> int;

[[nodiscard]] auto
closest_structure_surface(const Engine::Core::Entity& structure,
                          const QVector3D& query_point) -> StructureSurfaceContact;

[[nodiscard]] auto structure_impact_point(const Engine::Core::Entity& structure,
                                          const QVector3D& source,
                                          float lateral_offset = 0.0F,
                                          float height_override = -1.0F) -> QVector3D;

[[nodiscard]] auto
structure_melee_approach(const Engine::Core::Entity& attacker,
                         const Engine::Core::Entity& structure) -> StructureApproach;

[[nodiscard]] auto
structure_navigation_melee_approach(const Engine::Core::Entity& attacker,
                                    const Engine::Core::Entity& structure,
                                    float extra_tolerance = 0.0F) -> StructureApproach;

[[nodiscard]] auto structure_melee_contact_active(const Engine::Core::Entity& attacker,
                                                  const Engine::Core::Entity& structure,
                                                  float extra_tolerance = 0.0F) -> bool;

[[nodiscard]] auto structure_surface_distance(const Engine::Core::Entity& structure,
                                              const QVector3D& point) -> float;

} // namespace Game::Systems::Combat
