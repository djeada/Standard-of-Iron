#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <string>
#include <vector>

#include "anim_key.h"
#include "material.h"
#include "scene_renderer.h"

namespace Render::GL {

class Mesh;
class Texture;
class Shader;
struct AnimationInputs;

struct RecordedMeshCmd {
  Mesh* mesh{nullptr};
  Texture* texture{nullptr};
  Shader* shader{nullptr};

  const Material* material{nullptr};

  QMatrix4x4 local_model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
  int material_id{0};
};

struct PoseTemplate {
  std::vector<RecordedMeshCmd> commands;
};

class TemplateRecorder : public Renderer {
public:
  TemplateRecorder() = default;

  void reset(std::size_t reserve_hint = 0);

  auto take_commands() -> std::vector<RecordedMeshCmd> { return std::move(m_commands); }

  [[nodiscard]] auto commands() const -> const std::vector<RecordedMeshCmd>& {
    return m_commands;
  }

  void set_current_material(const Material* material) { m_current_material = material; }
  [[nodiscard]] auto get_current_material() const -> const Material* {
    return m_current_material;
  }

  void mesh(Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* texture = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override;
  void banner(Mesh* mesh,
              const QMatrix4x4& model,
              const QVector3D& color,
              const QVector3D& trim_color,
              Texture* texture = nullptr,
              float alpha = 1.0F,
              int material_id = 0) override;
  void part(Mesh* mesh,
            Material* material,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* texture = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override;
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
  void rigged(const RiggedCreatureCmd&) override {}

private:
  std::vector<RecordedMeshCmd> m_commands;
  const Material* m_current_material = nullptr;
};

auto make_animation_inputs(const AnimKey& key) -> AnimationInputs;

} // namespace Render::GL
