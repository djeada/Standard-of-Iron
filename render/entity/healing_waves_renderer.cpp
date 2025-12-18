#include "healing_waves_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/healing_beam.h"
#include "../../game/systems/healing_beam_system.h"
#include "../../game/systems/nation_id.h"
#include "../scene_renderer.h"
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

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

    // Get beam properties
    QVector3D start = beam->get_start();
    QVector3D end = beam->get_end();
    QVector3D color = beam->get_color();

    // Only render waves for blue (Roman) beams
    // Green beams (Carthage) are rendered by the regular beam renderer
    bool is_blue = (color.z() > 0.8F && color.x() < 0.5F); // Blue has high Z, low X
    if (!is_blue) {
      continue; // Skip non-blue beams
    }

    // Calculate direction and distance
    QVector3D direction = end - start;
    float distance = direction.length();
    if (distance < 0.01F) {
      continue;
    }
    direction.normalize();

    // Create wave effect - multiple wave particles traveling from healer to target
    constexpr int num_waves = 5;
    constexpr float wave_spacing = 0.15F;
    constexpr float wave_width = 0.08F;
    constexpr float wave_speed = 3.0F;

    for (int i = 0; i < num_waves; ++i) {
      // Calculate wave position along the path
      float wave_offset = (animation_time * wave_speed + i * wave_spacing);
      wave_offset = std::fmod(wave_offset, distance + wave_spacing * num_waves);
      
      if (wave_offset > distance) {
        continue; // Wave hasn't started yet
      }

      float wave_progress = wave_offset / distance;
      QVector3D wave_pos = start + direction * wave_offset;

      // Wave intensity fades in and out
      float wave_intensity = intensity;
      if (wave_progress < 0.2F) {
        wave_intensity *= wave_progress / 0.2F; // Fade in
      } else if (wave_progress > 0.8F) {
        wave_intensity *= (1.0F - wave_progress) / 0.2F; // Fade out
      }

      // Create oscillating wave shape - use perpendicular vectors
      QVector3D perpendicular1;
      if (std::abs(direction.y()) < 0.9F) {
        perpendicular1 = QVector3D::crossProduct(direction, QVector3D(0, 1, 0));
      } else {
        perpendicular1 = QVector3D::crossProduct(direction, QVector3D(1, 0, 0));
      }
      perpendicular1.normalize();
      QVector3D perpendicular2 = QVector3D::crossProduct(direction, perpendicular1);
      perpendicular2.normalize();

      // Draw wave as a series of particles in a wave pattern
      constexpr int wave_segments = 8;
      constexpr float pi = std::numbers::pi_v<float>;
      
      for (int seg = 0; seg < wave_segments; ++seg) {
        float angle = (static_cast<float>(seg) / wave_segments) * 2.0F * pi;
        float wave_phase = animation_time * 4.0F + i * 0.5F;
        float amplitude = wave_width * std::sin(wave_phase + angle);
        
        QVector3D particle_offset = perpendicular1 * std::cos(angle) * amplitude +
                                    perpendicular2 * std::sin(angle) * amplitude;
        QVector3D particle_pos = wave_pos + particle_offset;

        // Render wave particle using the healing beam renderer
        // Draw small beam segment to create wave effect
        QVector3D segment_start = particle_pos - direction * 0.05F;
        QVector3D segment_end = particle_pos + direction * 0.05F;
        
        renderer->healing_beam(segment_start, segment_end, color, 1.0F,
                              wave_width * 0.5F, wave_intensity, animation_time);
      }
    }
  }
}

} // namespace Render::GL
