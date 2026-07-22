#include "effects_submitter.h"

#include "draw_queue.h"
#include "scene_renderer.h"

namespace Render::GL {

void EffectsSubmitter::healing_beam(DrawQueue* queue,
                                    const QVector3D& start,
                                    const QVector3D& end,
                                    const QVector3D& color,
                                    float progress,
                                    float beam_width,
                                    float intensity,
                                    float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::HealingBeam;
  cmd.position = start;
  cmd.end_pos = end;
  cmd.color = color;
  cmd.progress = progress;
  cmd.beam_width = beam_width;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::High;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::healer_aura(DrawQueue* queue,
                                   const QVector3D& position,
                                   const QVector3D& color,
                                   float radius,
                                   float intensity,
                                   float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::HealerAura;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::Normal;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::combat_dust(DrawQueue* queue,
                                   const QVector3D& position,
                                   const QVector3D& color,
                                   float radius,
                                   float intensity,
                                   float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::CombatDust;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::Low;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::building_flame(DrawQueue* queue,
                                      const QVector3D& position,
                                      const QVector3D& color,
                                      float radius,
                                      float intensity,
                                      float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::BuildingFlame;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::Normal;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::burning_flame(DrawQueue* queue,
                                     const QVector3D& position,
                                     const QVector3D& color,
                                     float radius,
                                     float intensity,
                                     float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::BurningFlame;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::High;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::fireball(DrawQueue* queue,
                                const QVector3D& position,
                                const QVector3D& color,
                                float radius,
                                float intensity,
                                float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::Fireball;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::High;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::blood_pool(DrawQueue* queue,
                                  const QVector3D& position,
                                  float radius,
                                  float alpha_scale,
                                  float rotation,
                                  float aspect_ratio,
                                  float seed) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::BloodPool;
  cmd.position = position;
  cmd.radius = radius;
  cmd.alpha_scale = alpha_scale;
  cmd.rotation = rotation;
  cmd.aspect_ratio = aspect_ratio;
  cmd.seed = seed;
  cmd.priority = CommandPriority::Low;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::stone_impact(DrawQueue* queue,
                                    const QVector3D& position,
                                    const QVector3D& color,
                                    float radius,
                                    float intensity,
                                    float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::StoneImpact;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::High;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::metal_spark(DrawQueue* queue,
                                   const QVector3D& position,
                                   const QVector3D& color,
                                   float radius,
                                   float intensity,
                                   float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::MetalSpark;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::High;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

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
          position, radius, SubmissionFogMode::VisibleOnly)) {
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
          position, radius, SubmissionFogMode::VisibleOnly)) {
    return;
  }
  m_effects_submitter->combat_dust(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::building_flame(const QVector3D& position,
                              const QVector3D& color,
                              float radius,
                              float intensity,
                              float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly)) {
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
          position, radius, SubmissionFogMode::VisibleOnly)) {
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
          position, radius, SubmissionFogMode::VisibleOnly)) {
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
          position, radius, SubmissionFogMode::VisibleOnly)) {
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
          position, radius, SubmissionFogMode::VisibleOnly)) {
    return;
  }
  m_effects_submitter->stone_impact(
      m_active_queue, position, color, radius, intensity, time);
}

void Renderer::metal_spark(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time) {
  if (!m_submission_visibility.accepts_sphere(
          position, radius, SubmissionFogMode::VisibleOnly)) {
    return;
  }
  m_effects_submitter->metal_spark(
      m_active_queue, position, color, radius, intensity, time);
}

} // namespace Render::GL
