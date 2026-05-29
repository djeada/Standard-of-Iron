#pragma once

#include <QVector3D>

namespace Render::GL {

class DrawQueue;

class EffectsSubmitter {
public:
  void healing_beam(DrawQueue* queue,
                    const QVector3D& start,
                    const QVector3D& end,
                    const QVector3D& color,
                    float progress,
                    float beam_width,
                    float intensity,
                    float time) const;
  void healer_aura(DrawQueue* queue,
                   const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time) const;
  void combat_dust(DrawQueue* queue,
                   const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time) const;
  void building_flame(DrawQueue* queue,
                      const QVector3D& position,
                      const QVector3D& color,
                      float radius,
                      float intensity,
                      float time) const;
  void burning_flame(DrawQueue* queue,
                     const QVector3D& position,
                     const QVector3D& color,
                     float radius,
                     float intensity,
                     float time) const;
  void fireball(DrawQueue* queue,
                const QVector3D& position,
                const QVector3D& color,
                float radius,
                float intensity,
                float time) const;
  void blood_pool(DrawQueue* queue,
                  const QVector3D& position,
                  float radius,
                  float alpha_scale,
                  float rotation,
                  float aspect_ratio,
                  float seed) const;
  void stone_impact(DrawQueue* queue,
                    const QVector3D& position,
                    const QVector3D& color,
                    float radius,
                    float intensity,
                    float time) const;
  void metal_spark(DrawQueue* queue,
                   const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time) const;
};

} // namespace Render::GL
