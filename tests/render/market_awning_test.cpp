#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <gtest/gtest.h>

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "render/entity/marketplace_renderer_common.h"
#include "render/entity/registry.h"
#include "render/gl/mesh.h"
#include "render/submitter.h"

namespace {

class ClothRecorder final : public Render::GL::ISubmitter {
public:
  int cloth_parts{0};
  int other_parts{0};
  float signature{0.0F};

  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int material_id) override {
    if (mesh == nullptr) {
      return;
    }
    (material_id == 3 ? cloth_parts : other_parts) += 1;
    const QVector3D centre = model.map(QVector3D(0.0F, 0.0F, 0.0F));
    signature += centre.x() * 1.3F + centre.y() * 7.1F + centre.z() * 3.7F;
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
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
};

const std::array<Render::GL::MarketAwning, 1> k_awning{{
    Render::GL::MarketAwning{.back = QVector3D(0.0F, 0.9F, 0.4F),
                             .front = QVector3D(0.0F, 0.75F, -0.1F),
                             .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                             .half_width = 0.3F,
                             .stripe_a = QVector3D(0.6F, 0.1F, 0.1F),
                             .stripe_b = QVector3D(0.9F, 0.8F, 0.7F),
                             .stripes = 4},
}};
const std::array<Render::GL::MarketHanging, 1> k_hanging{{
    Render::GL::MarketHanging{.pivot = QVector3D(0.3F, 0.8F, 0.0F),
                              .length = 0.12F,
                              .radius = 0.02F,
                              .color = QVector3D(0.5F, 0.2F, 0.1F),
                              .beads = 2},
}};

auto record(float time, float distance_sq = 100.0F) -> ClothRecorder {
  Render::GL::DrawContext ctx{};
  ctx.animation_time = time;
  ctx.distance_sq = distance_sq;
  ClothRecorder out;
  Render::GL::submit_market_awnings(ctx, out, k_awning, k_hanging);
  return out;
}

} // namespace

TEST(MarketAwningTest, AwningsAreClothAndMoveInTheWind) {
  const auto now = record(3.0F);
  EXPECT_GT(now.cloth_parts, 20) << "stripes x segments plus a valance per stripe";
  EXPECT_GT(now.other_parts, 0) << "the hanging goods are drawn";
  const auto later = record(4.1F);
  EXPECT_EQ(now.cloth_parts, later.cloth_parts);
  EXPECT_NE(now.signature, later.signature) << "the awning is frozen";
}

TEST(MarketAwningTest, DistantMarketsSkipTheClothEntirely) {
  const auto far = record(3.0F, 200.0F * 200.0F);
  EXPECT_EQ(far.cloth_parts, 0);
  EXPECT_EQ(far.other_parts, 0);
}
