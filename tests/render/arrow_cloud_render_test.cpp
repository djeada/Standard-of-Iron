#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <utility>
#include <vector>

#include "game/systems/arrow_system.h"
#include "render/geom/arrow.h"
#include "render/scene_renderer.h"

namespace {

struct RecordedMeshDraw {
  Render::GL::Mesh* mesh{nullptr};
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
};

class RecordingRenderer final : public Render::GL::Renderer {
public:
  RecordingRenderer()
      : Renderer(Render::ShaderQuality::Full) {}

  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Render::GL::Texture*,
            float alpha,
            int) override {
    meshes.push_back({mesh, model, color, alpha});
  }

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

  std::vector<RecordedMeshDraw> meshes;
};

auto local_z_bounds(Render::GL::Mesh* mesh) -> std::pair<float, float> {
  float min_z = std::numeric_limits<float>::infinity();
  float max_z = -std::numeric_limits<float>::infinity();
  for (const auto& vertex : mesh->get_vertices()) {
    min_z = std::min(min_z, vertex.position[2]);
    max_z = std::max(max_z, vertex.position[2]);
  }
  return {min_z, max_z};
}

auto transformed_z_bounds(const RecordedMeshDraw& draw) -> std::pair<float, float> {
  float min_z = std::numeric_limits<float>::infinity();
  float max_z = -std::numeric_limits<float>::infinity();
  for (const auto& vertex : draw.mesh->get_vertices()) {
    QVector3D const pos = draw.model.map(
        QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
    min_z = std::min(min_z, pos.z());
    max_z = std::max(max_z, pos.z());
  }
  return {min_z, max_z};
}

auto luminance(const QVector3D& color) -> float {
  return 0.2126F * color.x() + 0.7152F * color.y() + 0.0722F * color.z();
}

auto find_draw(const std::vector<RecordedMeshDraw>& meshes,
               Render::GL::Mesh* target) -> const RecordedMeshDraw* {
  for (const auto& draw : meshes) {
    if (draw.mesh == target) {
      return &draw;
    }
  }
  return nullptr;
}

TEST(ArrowCloudRender, MarkerArrowUsesTwentyPercentShorterMeshSpan) {
  Game::Systems::ArrowSystem system;
  system.spawn_arrow(QVector3D(0.0F, 0.0F, 0.0F),
                     QVector3D(0.0F, 0.0F, 10.0F),
                     QVector3D(0.75F, 0.12F, 0.08F),
                     8.0F,
                     Game::Systems::ArrowVisualStyle::Marker);
  system.update(nullptr, 0.625F);

  ASSERT_EQ(system.arrows().size(), 1U);
  const auto& arrow = system.arrows().front();

  RecordingRenderer renderer;
  Render::GL::render_arrows(
      &renderer, reinterpret_cast<Render::GL::ResourceManager*>(0x1), system);

  auto* shaft_mesh = Render::Geom::Arrow::get_shaft();
  auto* tip_mesh = Render::Geom::Arrow::get_tip();
  ASSERT_NE(shaft_mesh, nullptr);
  ASSERT_NE(tip_mesh, nullptr);
  ASSERT_EQ(renderer.meshes.size(), 2U);

  const RecordedMeshDraw* shaft_draw = find_draw(renderer.meshes, shaft_mesh);
  const RecordedMeshDraw* tip_draw = find_draw(renderer.meshes, tip_mesh);
  ASSERT_NE(shaft_draw, nullptr);
  ASSERT_NE(tip_draw, nullptr);

  const auto [shaft_min_z, shaft_max_z] = transformed_z_bounds(*shaft_draw);
  const auto [tip_min_z, tip_max_z] = transformed_z_bounds(*tip_draw);
  float const rendered_length = tip_max_z - shaft_min_z;
  float const expected_length = Render::Geom::Arrow::k_total_length *
                                Render::Geom::Arrow::k_arrow_z_scale * arrow.scale;

  EXPECT_NEAR(shaft_max_z, tip_min_z, 1.0e-4F);
  EXPECT_NEAR(rendered_length, expected_length, 1.0e-4F);
  EXPECT_NEAR(rendered_length / (Render::Geom::Arrow::k_arrow_z_scale * arrow.scale),
              0.8F,
              1.0e-4F);
}

TEST(ArrowCloudRender, MarkerArrowTipUsesBrightMetallicColor) {
  Game::Systems::ArrowSystem system;
  system.spawn_arrow(QVector3D(0.0F, 0.0F, 0.0F),
                     QVector3D(0.0F, 0.0F, 10.0F),
                     QVector3D(0.12F, 0.24F, 0.72F),
                     8.0F,
                     Game::Systems::ArrowVisualStyle::Marker);
  system.update(nullptr, 0.625F);

  ASSERT_EQ(system.arrows().size(), 1U);
  const auto& arrow = system.arrows().front();

  RecordingRenderer renderer;
  Render::GL::render_arrows(
      &renderer, reinterpret_cast<Render::GL::ResourceManager*>(0x1), system);

  auto* shaft_mesh = Render::Geom::Arrow::get_shaft();
  auto* tip_mesh = Render::Geom::Arrow::get_tip();
  ASSERT_NE(shaft_mesh, nullptr);
  ASSERT_NE(tip_mesh, nullptr);

  const RecordedMeshDraw* shaft_draw = find_draw(renderer.meshes, shaft_mesh);
  const RecordedMeshDraw* tip_draw = find_draw(renderer.meshes, tip_mesh);
  ASSERT_NE(shaft_draw, nullptr);
  ASSERT_NE(tip_draw, nullptr);

  QVector3D const expected_tip_color =
      Render::Geom::Arrow::tip_color(0.94F + (arrow.brightness * 0.12F));
  EXPECT_NEAR(tip_draw->color.x(), expected_tip_color.x(), 1.0e-4F);
  EXPECT_NEAR(tip_draw->color.y(), expected_tip_color.y(), 1.0e-4F);
  EXPECT_NEAR(tip_draw->color.z(), expected_tip_color.z(), 1.0e-4F);
  EXPECT_GT(luminance(tip_draw->color), luminance(shaft_draw->color));
  EXPECT_NEAR(tip_draw->color.x(), tip_draw->color.y(), 0.03F);
  EXPECT_NEAR(tip_draw->color.y(), tip_draw->color.z(), 0.05F);

  const auto [shaft_local_min_z, shaft_local_max_z] = local_z_bounds(shaft_mesh);
  const auto [tip_local_min_z, tip_local_max_z] = local_z_bounds(tip_mesh);
  EXPECT_NEAR(shaft_local_min_z, 0.0F, 1.0e-5F);
  EXPECT_NEAR(shaft_local_max_z, Render::Geom::Arrow::k_shaft_length, 1.0e-5F);
  EXPECT_NEAR(tip_local_min_z, Render::Geom::Arrow::k_shaft_length, 1.0e-5F);
  EXPECT_NEAR(tip_local_max_z, Render::Geom::Arrow::k_total_length, 1.0e-5F);
  EXPECT_GT(tip_local_max_z - tip_local_min_z, 0.15F);
}

} // namespace
