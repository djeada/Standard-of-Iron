#include "healing_waves_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/healing_beam.h"
#include "../../game/systems/healing_beam_system.h"
#include "../../game/systems/healing_colors.h"
#include "../../game/systems/nation_id.h"
#include "../scene_renderer.h"
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

namespace {

constexpr int k_num_waves = 3;
constexpr float k_wave_spacing = 0.3F;
constexpr float k_wave_speed = 2.5F;
constexpr float k_wave_width = 0.12F;
constexpr int k_wave_ribbons = 6;
constexpr float k_ribbon_radius = 0.08F;
constexpr int k_segments_per_wave = 10;
constexpr float k_spiral_twist_rate = 8.0F;
constexpr float k_edge_fade_factor = 1.5F;
} 

void render_healing_waves(Renderer *renderer, ResourceManager *,
                          const Game::Systems::HealingBeamSystem &beam_system) {
  if (renderer == nullptr || beam_system.get_beam_count() == 0) {
    return;
  }

  float animation_time = renderer->get_animation_time();

  for (const auto &beam : beam_system.get_beams()) {
    if (!beam || !beam->is_active()) {
      continue;
    }

    float intensity = beam->get_intensity();
    if (intensity < 0.01F) {
      continue;
    }

    QVector3D start = beam->get_start();
    QVector3D end = beam->get_end();
    QVector3D color = beam->get_color();

    if (!Game::Systems::is_roman_healing_color(color)) {
      continue;
    }

    QVector3D direction = end - start;
    float distance = direction.length();
    if (distance < 0.01F) {
      continue;
    }
    direction.normalize();

    QVector3D perpendicular1;
    if (std::abs(direction.y()) < 0.9F) {
      perpendicular1 = QVector3D::crossProduct(direction, QVector3D(0, 1, 0));
    } else {
      perpendicular1 = QVector3D::crossProduct(direction, QVector3D(1, 0, 0));
    }
    perpendicular1.normalize();
    QVector3D perpendicular2 =
        QVector3D::crossProduct(direction, perpendicular1);
    perpendicular2.normalize();

    constexpr float pi = std::numbers::pi_v<float>;

    for (int wave_idx = 0; wave_idx < k_num_waves; ++wave_idx) {

      float wave_cycle_time =
          animation_time * k_wave_speed + wave_idx * k_wave_spacing;
      float wave_offset =
          std::fmod(wave_cycle_time, distance + k_wave_spacing * k_num_waves);

      if (wave_offset > distance) {
        continue;
      }

      float wave_progress = wave_offset / distance;
      QVector3D wave_center = start + direction * wave_offset;

      float wave_intensity = intensity;
      if (wave_progress < 0.15F) {
        wave_intensity *= wave_progress / 0.15F;
      } else if (wave_progress > 0.85F) {
        wave_intensity *= (1.0F - wave_progress) / 0.15F;
      }

      for (int ribbon = 0; ribbon < k_wave_ribbons; ++ribbon) {
        float ribbon_angle_offset =
            (static_cast<float>(ribbon) / k_wave_ribbons) * 2.0F * pi;

        for (int seg = 0; seg < k_segments_per_wave; ++seg) {
          float seg_t = static_cast<float>(seg) / k_segments_per_wave;
          float next_seg_t = static_cast<float>(seg + 1) / k_segments_per_wave;

          float seg_dist = (seg_t - 0.5F) * k_wave_width;
          float next_seg_dist = (next_seg_t - 0.5F) * k_wave_width;

          float spiral_phase = animation_time * 3.0F + wave_idx * pi;
          float seg_angle = ribbon_angle_offset +
                            seg_dist * k_spiral_twist_rate + spiral_phase;
          float next_seg_angle = ribbon_angle_offset +
                                 next_seg_dist * k_spiral_twist_rate +
                                 spiral_phase;

          float seg_radius =
              k_ribbon_radius * (1.0F - std::abs(seg_t - 0.5F) * 2.0F);
          float next_seg_radius =
              k_ribbon_radius * (1.0F - std::abs(next_seg_t - 0.5F) * 2.0F);

          QVector3D seg_offset =
              perpendicular1 * std::cos(seg_angle) * seg_radius +
              perpendicular2 * std::sin(seg_angle) * seg_radius;
          QVector3D next_seg_offset =
              perpendicular1 * std::cos(next_seg_angle) * next_seg_radius +
              perpendicular2 * std::sin(next_seg_angle) * next_seg_radius;

          QVector3D seg_pos = wave_center + direction * seg_dist + seg_offset;
          QVector3D next_seg_pos =
              wave_center + direction * next_seg_dist + next_seg_offset;

          float seg_intensity =
              wave_intensity *
              (1.0F - std::abs(seg_t - 0.5F) * k_edge_fade_factor);
          seg_intensity = std::max(0.0F, seg_intensity);

          if (seg_intensity > 0.01F) {
            renderer->healing_beam(seg_pos, next_seg_pos, color, 1.0F, 0.04F,
                                   seg_intensity, animation_time);
          }
        }
      }
    }
  }
}

} 
