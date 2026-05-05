#include "shadow_batch.h"

#include "../../scene_renderer.h"

#include <QVector3D>

namespace Render::Creature::Pipeline {

void HumanoidShadowBatch::init(Render::GL::Shader *shader,
                                Render::GL::Mesh *mesh,
                                QVector2D light_dir) noexcept {
  shader_ = shader;
  mesh_ = mesh;
  light_dir_ = light_dir;
}

void HumanoidShadowBatch::add(QMatrix4x4 model, float alpha,
                               RenderPassIntent pass) noexcept {
  instances_.push_back({std::move(model), alpha, pass});
}

void HumanoidShadowBatch::clear() noexcept {
  shader_ = nullptr;
  mesh_ = nullptr;
  light_dir_ = {};
  instances_.clear();
}

void flush_shadow_batch(HumanoidShadowBatch &batch,
                        Render::GL::ISubmitter &out) {
  if (batch.empty() || batch.shader() == nullptr || batch.mesh() == nullptr) {
    return;
  }

  auto *renderer = dynamic_cast<Render::GL::Renderer *>(&out);
  if (renderer == nullptr) {
    return;
  }

  Render::GL::Shader *previous_shader = renderer->get_current_shader();
  renderer->set_current_shader(batch.shader());
  batch.shader()->set_uniform(QStringLiteral("u_lightDir"), batch.light_dir());

  for (const auto &inst : batch.instances()) {
    if (inst.pass == RenderPassIntent::Shadow) {
      continue;
    }
    out.mesh(batch.mesh(), inst.model, QVector3D(0.0F, 0.0F, 0.0F), nullptr,
             inst.alpha, 0);
  }

  renderer->set_current_shader(previous_shader);
}

} // namespace Render::Creature::Pipeline
