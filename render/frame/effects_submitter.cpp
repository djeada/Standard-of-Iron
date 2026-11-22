#include "render/frame/effects_submitter.h"

#include "render/frame/draw_queue.h"

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

void EffectsSubmitter::hearth_smoke(DrawQueue* queue,
                                    const QVector3D& position,
                                    const QVector3D& color,
                                    float radius,
                                    float intensity,
                                    float time) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::HearthSmoke;
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
                                   float time,
                                   const QVector3D& direction) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::MetalSpark;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.direction = direction;
  cmd.priority = CommandPriority::High;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

void EffectsSubmitter::weapon_arc(DrawQueue* queue,
                                  const QVector3D& position,
                                  const QVector3D& color,
                                  float radius,
                                  float intensity,
                                  float time,
                                  const QVector3D& direction,
                                  float span,
                                  float tilt) const {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::WeaponArc;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.direction = direction;
  cmd.span = span;
  cmd.tilt = tilt;
  cmd.priority = CommandPriority::High;
  if (queue != nullptr) {
    queue->submit(std::move(cmd));
  }
}

} // namespace Render::GL
