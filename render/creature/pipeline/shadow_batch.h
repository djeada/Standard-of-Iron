#pragma once

#include "render_pass_intent.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <cstddef>
#include <vector>

namespace Render::GL {
class Shader;
class Mesh;
class ISubmitter;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

/// Per-instance data for a single shadow quad.
struct ShadowInstance {
  QMatrix4x4 model{};
  float alpha{0.0F};
  RenderPassIntent pass{RenderPassIntent::Main};
};

/// Centralised shadow batch: shared state (shader, mesh, light direction) is
/// stored once and reused across all instances, avoiding redundant GPU state
/// switches and uniform uploads when shadows are flushed.
class HumanoidShadowBatch {
public:
  /// Initialise the shared render resources for this batch.
  /// Call once per frame before the first @ref add().
  void init(Render::GL::Shader *shader, Render::GL::Mesh *mesh,
            QVector2D light_dir) noexcept;

  /// Append a new shadow instance to the batch.
  void add(QMatrix4x4 model, float alpha,
           RenderPassIntent pass = RenderPassIntent::Main) noexcept;

  [[nodiscard]] auto empty() const noexcept -> bool {
    return instances_.empty();
  }

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return instances_.size();
  }

  void clear() noexcept;

  [[nodiscard]] auto shader() const noexcept -> Render::GL::Shader * {
    return shader_;
  }
  [[nodiscard]] auto mesh() const noexcept -> Render::GL::Mesh * {
    return mesh_;
  }
  [[nodiscard]] auto light_dir() const noexcept -> QVector2D {
    return light_dir_;
  }
  [[nodiscard]] auto instances() const noexcept
      -> const std::vector<ShadowInstance> & {
    return instances_;
  }

private:
  Render::GL::Shader *shader_{nullptr};
  Render::GL::Mesh *mesh_{nullptr};
  QVector2D light_dir_{};
  std::vector<ShadowInstance> instances_{};
};

/// Flush all pending shadow instances to @p out.
///
/// The shadow shader is bound once, @c u_lightDir is uploaded once, and every
/// Main-pass instance is drawn in sequence.  Shadow-pass instances (template
/// prewarm) are silently skipped, matching the existing per-lambda behaviour.
void flush_shadow_batch(HumanoidShadowBatch &batch, Render::GL::ISubmitter &out);

} // namespace Render::Creature::Pipeline
