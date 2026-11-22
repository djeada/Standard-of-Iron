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

namespace Render::GL::combat_dust_detail {

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
    Render::GL::FrameSubmitter* renderer,
    const Engine::Core::CommanderSignaturePresentationComponent::Entry& entry,
    const QVector3D& contact,
    const QVector3D& forward,
    const QVector3D& accent,
    float progress) {
  constexpr float k_cut_tilt = 0.62F;
  constexpr float k_slam_tilt = 1.02F;
  float const reach = std::max(1.2F, entry.reach);
  float const arc_alpha = entry.intensity * 0.90F;
  QVector3D const hot = accent * 0.65F + QVector3D(0.35F, 0.37F, 0.40F);
  QVector3D const across(-forward.z(), 0.0F, forward.x());

  auto cutting_wake =
      [&](const QVector3D& origin, float radius, float span, float tilt) {
        if (progress > 0.075F) {
          renderer->weapon_arc(origin - forward * 0.055F +
                                   QVector3D(0.0F, -0.045F, 0.0F),
                               accent * 0.65F + QVector3D(0.16F, 0.25F, 0.42F),
                               radius * 0.91F,
                               arc_alpha * 0.38F,
                               std::clamp((progress - 0.075F) / 0.925F, 0.0F, 1.0F),
                               forward,
                               span * 0.92F,
                               tilt);
        }
        if (progress > 0.08F && progress < 0.72F) {
          const float travel = std::clamp(progress / 0.36F, 0.0F, 1.0F);
          const float eased = travel * travel * (3.0F - 2.0F * travel);
          const float sign = span < 0.0F ? -1.0F : 1.0F;
          const float angle =
              (eased - 0.5F) * 2.0F * std::numbers::pi_v<float> * std::abs(span) * sign;
          const QVector3D cut_side =
              -across * std::cos(tilt) + QVector3D(0.0F, std::sin(tilt), 0.0F);
          const QVector3D ray = forward * std::cos(angle) + cut_side * std::sin(angle);
          const QVector3D tangent =
              (cut_side * std::cos(angle) - forward * std::sin(angle)) * sign;
          renderer->metal_spark(origin + ray * radius * 0.96F,
                                hot,
                                0.16F,
                                arc_alpha * (1.0F - progress) * 0.85F,
                                0.045F + progress * 0.13F,
                                tangent);
        }
      };

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
    cutting_wake(contact, reach * 1.05F, entry.span, tilt);
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
    cutting_wake(
        contact - QVector3D(0.0F, 0.22F, 0.0F), reach * 0.92F, entry.span, 0.0F);
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
    cutting_wake(contact - forward * (reach * 0.12F),
                 reach * 0.95F,
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
    float const drive =
        1.0F - std::pow(1.0F - std::clamp(progress * 1.7F, 0.0F, 1.0F), 3.0F);
    QVector3D const tip = contact + forward * (reach * (0.12F + 0.68F * drive));
    float const flash = (1.0F - progress) * arc_alpha;

    for (int streak = 0; streak < 4; ++streak) {
      const float lag = static_cast<float>(streak);
      const float delay = lag * 0.018F;
      if (entry.age < delay) {
        continue;
      }
      renderer->metal_spark(tip - forward * (reach * lag * 0.11F),
                            streak == 0 ? QVector3D(0.82F, 0.94F, 1.0F) : hot,
                            0.23F - lag * 0.033F,
                            flash * (1.65F - lag * 0.28F),
                            std::max(0.0F, entry.age - delay),
                            forward);
    }

    if (progress > 0.12F) {
      renderer->weapon_arc(tip - forward * 0.12F,
                           accent,
                           0.25F + reach * 0.12F,
                           flash * 0.55F,
                           (progress - 0.12F) / 0.88F,
                           across,
                           1.0F,
                           std::numbers::pi_v<float> * 0.5F);
    }
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
      0.38F * arc_alpha * (1.0F - progress) * std::clamp(progress * 9.0F, 0.0F, 1.0F);
  renderer->local_light(swing_light);
}

} // namespace Render::GL::combat_dust_detail
