#pragma once

#include <QVector3D>

#include "render/submitter.h"

namespace Render::GL {

inline constexpr float k_unseen_chroma = 0.70F;
inline constexpr QVector3D k_unseen_shade{0.32F, 0.33F, 0.36F};
inline constexpr QVector3D k_unseen_lift{0.040F, 0.046F, 0.058F};

[[nodiscard]] inline auto unseen_surface_color(const QVector3D& color) -> QVector3D {
  const float luminance =
      (0.2126F * color.x()) + (0.7152F * color.y()) + (0.0722F * color.z());
  const float grey = luminance * (1.0F - k_unseen_chroma);
  return {(grey + color.x() * k_unseen_chroma) * k_unseen_shade.x() + k_unseen_lift.x(),
          (grey + color.y() * k_unseen_chroma) * k_unseen_shade.y() + k_unseen_lift.y(),
          (grey + color.z() * k_unseen_chroma) * k_unseen_shade.z() +
              k_unseen_lift.z()};
}

class UnseenSubmitter : public ForwardingSubmitter {
public:
  using ForwardingSubmitter::ForwardingSubmitter;

  void mesh(Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* tex = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override {
    ForwardingSubmitter::mesh(
        mesh, model, unseen_surface_color(color), tex, alpha, material_id);
  }

  void banner(Mesh* mesh,
              const QMatrix4x4& model,
              const QVector3D& color,
              const QVector3D& trim_color,
              Texture* tex = nullptr,
              float alpha = 1.0F,
              int material_id = 0) override {
    ForwardingSubmitter::banner(mesh,
                                model,
                                unseen_surface_color(color),
                                unseen_surface_color(trim_color),
                                tex,
                                alpha,
                                material_id);
  }

  void part(Mesh* mesh,
            Material* material,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* tex = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override {
    ForwardingSubmitter::part(
        mesh, material, model, unseen_surface_color(color), tex, alpha, material_id);
  }

  void cylinder(const QVector3D& start,
                const QVector3D& end,
                float radius,
                const QVector3D& color,
                float alpha = 1.0F) override {
    ForwardingSubmitter::cylinder(
        start, end, radius, unseen_surface_color(color), alpha);
  }
};

} // namespace Render::GL
