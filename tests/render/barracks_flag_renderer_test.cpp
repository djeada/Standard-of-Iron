#include "render/entity/barracks_flag_renderer.h"

#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

struct RecordedMesh {
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  int material_id{0};
};

class RecordingSubmitter final : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;

  void mesh(Render::GL::Mesh *mesh, const QMatrix4x4 &model,
            const QVector3D &color, Render::GL::Texture *, float,
            int material_id) override {
    meshes.push_back({mesh, model, color, material_id});
  }

  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}
};

auto fake_mesh(int id) -> Render::GL::Mesh * {
  return reinterpret_cast<Render::GL::Mesh *>(static_cast<intptr_t>(id));
}

auto fake_texture(int id) -> Render::GL::Texture * {
  return reinterpret_cast<Render::GL::Texture *>(static_cast<intptr_t>(id));
}

TEST(BarracksFlagRenderer, DrawsHangingBannerPieces) {
  using namespace Render::GL;

  Engine::Core::Entity entity(1);
  DrawContext ctx;
  ctx.entity = &entity;

  RecordingSubmitter submitter;
  BarracksFlagRenderer::draw_hanging_banner(
      ctx, submitter, fake_mesh(1), fake_texture(2),
      QVector3D(0.8F, 0.2F, 0.1F), QVector3D(0.5F, 0.2F, 0.1F),
      {.pole_base = QVector3D(0.0F, 0.0F, -2.0F),
       .pole_color = QVector3D(0.3F, 0.2F, 0.1F),
       .beam_color = QVector3D(0.3F, 0.2F, 0.1F),
       .connector_color = QVector3D(0.8F, 0.8F, 0.8F),
       .ornament_offset = QVector3D(0.0F, 3.2F, 0.0F),
       .ornament_size = QVector3D(0.12F, 0.10F, 0.12F),
       .ornament_color = QVector3D(0.9F, 0.8F, 0.2F),
       .ring_count = 3,
       .ring_y_start = 0.5F,
       .ring_spacing = 0.6F,
       .ring_height = 0.03F,
       .ring_radius_scale = 2.2F,
       .ring_color = QVector3D(0.9F, 0.8F, 0.2F)});

  ASSERT_EQ(submitter.meshes.size(), 8u);
  EXPECT_EQ(submitter.meshes[0].mesh, fake_mesh(1));
  EXPECT_EQ(submitter.meshes[3].mesh, fake_mesh(1));
  EXPECT_EQ(submitter.meshes[4].mesh, fake_mesh(1));
  EXPECT_EQ(submitter.meshes[1].mesh, get_unit_cylinder());
  EXPECT_EQ(submitter.meshes[2].mesh, get_unit_cylinder());
  EXPECT_EQ(submitter.meshes[5].mesh, get_unit_cylinder());
}

TEST(BarracksFlagRenderer, UsesClothMeshWhenAvailable) {
  using namespace Render::GL;

  Engine::Core::Entity entity(2);
  DrawContext ctx;
  ctx.entity = &entity;

  RecordingSubmitter submitter;
  BarracksFlagRenderer::ClothBannerResources cloth{
      .cloth_mesh = fake_mesh(99),
      .banner_shader = reinterpret_cast<Shader *>(static_cast<intptr_t>(123))};
  BarracksFlagRenderer::draw_hanging_banner(
      ctx, submitter, fake_mesh(1), fake_texture(2),
      QVector3D(0.8F, 0.2F, 0.1F), QVector3D(0.5F, 0.2F, 0.1F),
      {.pole_base = QVector3D(2.0F, 0.0F, -1.5F),
       .pole_color = QVector3D(0.3F, 0.2F, 0.1F),
       .beam_color = QVector3D(0.3F, 0.2F, 0.1F),
       .connector_color = QVector3D(0.8F, 0.8F, 0.8F),
       .ornament_offset = QVector3D(0.0F, 2.8F, 0.0F),
       .ornament_size = QVector3D(0.1F, 0.1F, 0.1F),
       .ornament_color = QVector3D(0.9F, 0.8F, 0.2F),
       .ring_count = 0,
       .ring_color = QVector3D(0.9F, 0.8F, 0.2F)},
      &cloth);

  ASSERT_GE(submitter.meshes.size(), 5u);
  EXPECT_EQ(submitter.meshes[3].mesh, fake_mesh(99));
}

} // namespace
