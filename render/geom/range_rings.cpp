#include "range_rings.h"

#include <vector>

#include "../scene_renderer.h"

namespace Render::GL {

namespace {

constexpr float k_focused_thickness = 0.16F;
constexpr float k_group_thickness = 0.10F;
constexpr float k_focused_alpha = 0.85F;
constexpr float k_group_alpha = 0.5F;
constexpr float k_minimum_alpha_scale = 0.9F;
constexpr float k_minimum_phase = 1.6F;

const QVector3D k_max_range_color(0.90F, 0.82F, 0.46F);
const QVector3D k_min_range_color(0.86F, 0.34F, 0.24F);

auto marker_pattern(RangeRingPattern pattern) -> Game::Accessibility::TeamPattern {
  switch (pattern) {
  case RangeRingPattern::Dashed:
    return Game::Accessibility::TeamPattern::Dashed;
  case RangeRingPattern::Dotted:
    return Game::Accessibility::TeamPattern::Dotted;
  case RangeRingPattern::Ticked:
    return Game::Accessibility::TeamPattern::Chevron;
  case RangeRingPattern::Solid:
    break;
  }
  return Game::Accessibility::TeamPattern::Solid;
}

} // namespace

auto range_ring_pattern_for(Game::Systems::RangeWeaponClass weapon_class)
    -> RangeRingPattern {
  switch (weapon_class) {
  case Game::Systems::RangeWeaponClass::Bow:
    return RangeRingPattern::Dashed;
  case Game::Systems::RangeWeaponClass::Siege:
    return RangeRingPattern::Ticked;
  case Game::Systems::RangeWeaponClass::Arcane:
    return RangeRingPattern::Dotted;
  case Game::Systems::RangeWeaponClass::None:
    break;
  }
  return RangeRingPattern::Solid;
}

void render_attack_range_rings(Renderer* renderer,
                               ResourceManager* resources,
                               std::span<const Game::Systems::AttackRangeRing> rings) {
  if (rings.empty()) {
    return;
  }

  std::vector<RangeRingSpec> specs;
  specs.reserve(rings.size() * 2U);
  for (const auto& ring : rings) {
    const QVector3D center(ring.world_x, ring.world_y, ring.world_z);
    specs.push_back({.center = center,
                     .radius = ring.max_radius,
                     .pattern = range_ring_pattern_for(ring.weapon_class),
                     .focused = ring.focused,
                     .minimum = false});
    if (ring.min_radius > 0.0F) {
      specs.push_back({.center = center,
                       .radius = ring.min_radius,
                       .pattern = RangeRingPattern::Dashed,
                       .focused = ring.focused,
                       .minimum = true});
    }
  }
  render_range_rings(renderer, resources, specs);
}

void render_range_rings(Renderer* renderer,
                        ResourceManager* resources,
                        std::span<const RangeRingSpec> rings) {
  if ((renderer == nullptr) || (resources == nullptr) || rings.empty()) {
    return;
  }

  for (const auto& ring : rings) {
    if (ring.radius <= 0.0F) {
      continue;
    }

    GroundMarkerCmd marker;
    marker.center = ring.center;
    marker.outer_radius = ring.radius;
    marker.thickness = ring.focused ? k_focused_thickness : k_group_thickness;
    marker.color = ring.minimum ? k_min_range_color : k_max_range_color;
    marker.alpha = (ring.focused ? k_focused_alpha : k_group_alpha) *
                   (ring.minimum ? k_minimum_alpha_scale : 1.0F);
    marker.pattern = marker_pattern(ring.pattern);
    marker.focused = ring.focused;
    marker.phase = ring.minimum ? k_minimum_phase : 0.0F;
    renderer->ground_marker(marker);
  }
}

} // namespace Render::GL
