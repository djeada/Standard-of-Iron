#include <QVector3D>

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <limits>

#include "render/equipment/equipment_submit.h"
#include "render/equipment/helmets/helmet_alignment.h"
#include "render/equipment/helmets/roman_light_helmet.h"

namespace {

using Render::GL::BodyFrames;
using Render::GL::DrawContext;
using Render::GL::EquipmentBatch;
using Render::GL::HumanoidAnimationContext;
using Render::GL::HumanoidPalette;
using Render::GL::ISubmitter;
using Render::GL::RomanLightHelmetConfig;
using Render::GL::RomanLightHelmetRenderer;

struct AABB {
  QVector3D mn{std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity()};
  QVector3D mx{-std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity()};

  void include(const QVector3D& p) {
    mn.setX(std::min(mn.x(), p.x()));
    mn.setY(std::min(mn.y(), p.y()));
    mn.setZ(std::min(mn.z(), p.z()));
    mx.setX(std::max(mx.x(), p.x()));
    mx.setY(std::max(mx.y(), p.y()));
    mx.setZ(std::max(mx.z(), p.z()));
  }
};

class CountingSubmitter : public ISubmitter {
public:
  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture* = nullptr,
            float = 1.0F,
            int = 0) override {
    ++draw_count;
  }

  void cylinder(const QVector3D&,
                const QVector3D&,
                float,
                const QVector3D&,
                float = 1.0F) override {
    ++draw_count;
  }

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
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float = 1.0F) override {
  }

  int draw_count{0};
};

auto draw_count_of(const EquipmentBatch& batch) -> int {
  CountingSubmitter submitter;
  Render::GL::submit_equipment_batch(batch, submitter);
  return submitter.draw_count;
}

auto create_test_context() -> DrawContext {
  DrawContext ctx{};
  ctx.model.setToIdentity();
  return ctx;
}

auto create_test_frames() -> BodyFrames {
  using HP = Render::GL::HumanProportions;
  BodyFrames frames{};
  frames.head.origin = QVector3D(0.0F, HP::HEAD_CENTER_Y, 0.0F);
  frames.head.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.head.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.head.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.head.radius = HP::HEAD_RADIUS * 1.05F;
  return frames;
}

auto create_test_palette() -> HumanoidPalette {
  HumanoidPalette palette{};
  palette.cloth = QVector3D(0.64F, 0.18F, 0.16F);
  palette.leather_dark = QVector3D(0.18F, 0.12F, 0.10F);
  palette.metal = QVector3D(0.72F, 0.74F, 0.78F);
  return palette;
}

auto archetype_local_aabb(const Render::GL::RenderArchetype& archetype) -> AABB {
  AABB box;
  const auto& slice = archetype.lods[0];
  for (const auto& draw : slice.draws) {
    if (draw.mesh == nullptr) {
      continue;
    }
    for (const auto& v : draw.mesh->get_vertices()) {
      QVector3D const p{v.position[0], v.position[1], v.position[2]};
      box.include(draw.local_model.map(p));
    }
  }
  return box;
}

TEST(RomanLightHelmetMesh, ArchetypeHasDarkFantasySilhouetteBounds) {
  const auto& archetype = Render::GL::roman_light_helmet_archetype();
  ASSERT_GE(archetype.lods[0].draws.size(), 20U);

  const AABB box = archetype_local_aabb(archetype);
  EXPECT_LT(box.mn.y(), -0.60F);
  EXPECT_GT(box.mx.y(), 2.25F);
  EXPECT_LT(box.mn.x(), -1.30F);
  EXPECT_GT(box.mx.x(), 1.30F);
  EXPECT_LT(box.mn.z(), -1.18F);
  EXPECT_GT(box.mx.z(), 0.95F);
}

TEST(RomanLightHelmetMesh, SubmitProducesSingleRichAttachment) {
  EquipmentBatch batch;
  RomanLightHelmetRenderer::submit(RomanLightHelmetConfig{},
                                   create_test_context(),
                                   create_test_frames(),
                                   create_test_palette(),
                                   HumanoidAnimationContext{},
                                   batch);

  ASSERT_EQ(batch.archetypes.size(), 1U);
  EXPECT_GE(draw_count_of(batch), 20);

  auto const world = batch.archetypes.front().world;
  float const expected_scale =
      create_test_frames().head.radius * Render::GL::k_helmet_uniform_scale;
  EXPECT_NEAR(world.column(0).toVector3D().length(), expected_scale, 1.0e-5F);
  EXPECT_NEAR(world.column(1).toVector3D().length(), expected_scale, 1.0e-5F);
  EXPECT_NEAR(world.column(2).toVector3D().length(), expected_scale, 1.0e-5F);
}

TEST(RomanLightHelmetMesh, ZeroHeadRadiusSkipsSubmit) {
  EquipmentBatch batch;
  auto frames = create_test_frames();
  frames.head.radius = 0.0F;

  RomanLightHelmetRenderer::submit(RomanLightHelmetConfig{},
                                   create_test_context(),
                                   frames,
                                   create_test_palette(),
                                   {},
                                   batch);

  EXPECT_TRUE(batch.archetypes.empty());
  EXPECT_EQ(draw_count_of(batch), 0);
}

TEST(RomanLightHelmetMesh, FillRoleColorsPreservesColdSteelAndCrimsonCrest) {
  std::array<QVector3D, Render::GL::k_roman_light_helmet_role_count> colors{};
  auto const count = Render::GL::roman_light_helmet_fill_role_colors(
      create_test_palette(), colors.data(), colors.size());

  ASSERT_EQ(count, Render::GL::k_roman_light_helmet_role_count);
  EXPECT_GT(colors[0].z(), colors[0].x());
  EXPECT_LT(colors[1].x(), colors[0].x());
  EXPECT_LT(colors[1].y(), colors[0].y());
  EXPECT_LT(colors[1].z(), colors[0].z());
  EXPECT_GT(colors[2].x(), colors[2].y());
  EXPECT_GT(colors[2].x(), colors[2].z());
}

} // namespace
