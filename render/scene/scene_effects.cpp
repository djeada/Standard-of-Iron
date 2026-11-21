#include "render/scene/scene_renderer.h"

#include "render/frame/effects_submitter.h"

namespace Render::GL {

void Renderer::healing_beam(const QVector3D& start,
                            const QVector3D& end,
                            const QVector3D& color,
                            float progress,
                            float beam_width,
                            float intensity,
                            float time) {
  if (!m_submission_visibility.accepts_segment(
          start, end, beam_width, SubmissionFogMode::VisibleOnly)) {
    return;
  }
  m_effects_submitter->healing_beam(
      m_active_queue, start, end, color, progress, beam_width, intensity, time);
}

void Renderer::healer_aura(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->healer_aura(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::combat_dust(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->combat_dust(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::hearth_smoke(const QVector3D& position,
                            const QVector3D& color,
                            float radius,
                            float intensity,
                            float time) {

  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->hearth_smoke(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::building_flame(const QVector3D& position,
                              const QVector3D& color,
                              float radius,
                              float intensity,
                              float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->building_flame(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::burning_flame(const QVector3D& position,
                             const QVector3D& color,
                             float radius,
                             float intensity,
                             float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->burning_flame(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::fireball(const QVector3D& position,
                        const QVector3D& color,
                        float radius,
                        float intensity,
                        float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->fireball(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::blood_pool(const QVector3D& position,
                          float radius,
                          float alpha_scale,
                          float rotation,
                          float aspect_ratio,
                          float seed) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->blood_pool(
      m_active_queue, position, radius, alpha_scale, rotation, aspect_ratio, seed);
}

void Renderer::stone_impact(const QVector3D& position,
                            const QVector3D& color,
                            float radius,
                            float intensity,
                            float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->stone_impact(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::metal_spark(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time,
                           const QVector3D& direction) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->metal_spark(
      m_active_queue, position, color, radius, intensity, time, direction);
}

void Renderer::weapon_arc(const QVector3D& position,
                          const QVector3D& color,
                          float radius,
                          float intensity,
                          float time,
                          const QVector3D& direction,
                          float span,
                          float tilt) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  m_effects_submitter->weapon_arc(
      m_active_queue, position, color, radius, intensity, time, direction, span, tilt);
}

} // namespace Render::GL
