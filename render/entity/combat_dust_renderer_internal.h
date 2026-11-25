#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "render/entity/building_collapse.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/frame/camera_visibility.h"
#include "render/frame/combat_dust_defaults.h"
#include "render/frame/frame_submitter.h"

namespace Render::GL::combat_dust_detail {

constexpr float k_visibility_check_radius = 3.0F;
constexpr float k_blood_y_offset = 0.02F;
constexpr float k_stone_impact_intensity = 1.5F;
constexpr float k_stone_impact_y_offset = 0.1F;
constexpr float k_stone_impact_duration = 10.0F;

inline auto blood_alpha_scale(float elapsed_time, float lifetime) -> float {
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

auto commander_strike_accent(Engine::Core::Entity* commander) -> QVector3D;

void draw_commander_swing(
    Render::GL::FrameSubmitter* renderer,
    const Engine::Core::CommanderSignaturePresentationComponent::Entry& entry,
    const QVector3D& contact,
    const QVector3D& forward,
    const QVector3D& accent,
    float progress);

} // namespace Render::GL::combat_dust_detail
