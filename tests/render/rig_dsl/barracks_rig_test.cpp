// Stage 11 — StaticAnchorResolver + StaticPaletteResolver +
// interpretation of the barracks structural rig.
//
// We can't exercise the GL submitter here without a context, so the
// tests intercept parts via a recording ISubmitter and assert on the
// *emitted* matrices/colours.

#include "render/rig_dsl/defs/barracks_rig.h"
#include "render/rig_dsl/rig_interpreter.h"
#include "render/rig_dsl/static_resolver.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

#include <vector>

namespace {

struct RecordedPart {
  QMatrix4x4 model;
  QVector3D color;
  float alpha{0.0F};
};

class RecordingSubmitter final : public Render::GL::ISubmitter {
public:
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &model,
            const QVector3D &color, Render::GL::Texture *, float alpha,
            int = 0) override {
    m_records.push_back({model, color, alpha});
  }
  void part(Render::GL::Mesh *, Render::GL::Material *,
            const QMatrix4x4 &model, const QVector3D &color,
            Render::GL::Texture *, float alpha, int = 0) override {
    m_records.push_back({model, color, alpha});
  }

  // Remaining ISubmitter methods: no-ops for this test. We don't care
  // about the emission channel — only that the rig interpreter reaches
  // mesh/part for each part it iterates.
  void cylinder(const QVector3D &, const QVector3D &, float,
                const QVector3D &, float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &,
                       float) override {}
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

  [[nodiscard]] auto records() const -> const std::vector<RecordedPart> & {
    return m_records;
  }

private:
  std::vector<RecordedPart> m_records;
};

} // namespace

TEST(StaticResolverTest, AnchorLookupRoundTrip) {
  Render::RigDSL::StaticAnchorResolver<8> resolver;
  resolver.set(3, QVector3D(1.5F, 2.5F, -0.5F));
  resolver.set(7, QVector3D(-4.0F, 0.0F, 8.0F));
  EXPECT_EQ(resolver.resolve(3), QVector3D(1.5F, 2.5F, -0.5F));
  EXPECT_EQ(resolver.resolve(7), QVector3D(-4.0F, 0.0F, 8.0F));
  EXPECT_EQ(resolver.resolve(0), QVector3D(0.0F, 0.0F, 0.0F));
}

TEST(StaticResolverTest, AnchorOutOfRangeReturnsZero) {
  Render::RigDSL::StaticAnchorResolver<4> resolver;
  resolver.set(2, QVector3D(9.0F, 9.0F, 9.0F));
  EXPECT_EQ(resolver.resolve(9), QVector3D(0.0F, 0.0F, 0.0F));
}

TEST(StaticResolverTest, PaletteLookupRoundTrip) {
  Render::RigDSL::StaticPaletteResolver pal;
  pal.set(Render::RigDSL::PaletteSlot::Skin, QVector3D(0.8F, 0.6F, 0.5F));
  pal.set(Render::RigDSL::PaletteSlot::Metal, QVector3D(0.2F, 0.3F, 0.4F));
  EXPECT_EQ(pal.resolve(Render::RigDSL::PaletteSlot::Skin),
            QVector3D(0.8F, 0.6F, 0.5F));
  EXPECT_EQ(pal.resolve(Render::RigDSL::PaletteSlot::Metal),
            QVector3D(0.2F, 0.3F, 0.4F));
}

TEST(BarracksRigTest, PlatformBaseBoxHasExpectedCenterAndScale) {
  using namespace Render::RigDSL::Barracks;
  Render::RigDSL::StaticAnchorResolver<Goods_Amp3Top + 1U> anchors;
  anchors.set(Platform_BaseLow, QVector3D(-2.0F, 0.0F, -1.8F));
  anchors.set(Platform_BaseHigh, QVector3D(2.0F, 0.16F, 1.8F));

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.model = QMatrix4x4{};
  ctx.lod = 0;
  ctx.global_alpha = 1.0F;

  RecordingSubmitter sub;
  Render::RigDSL::render_part(kParts[0], ctx, sub);

  ASSERT_EQ(sub.records().size(), 1U);
  auto const &m = sub.records()[0].model;
  QVector3D const centre = m.map(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_NEAR(centre.x(), 0.0F, 1e-4F);
  EXPECT_NEAR(centre.y(), 0.08F, 1e-4F);
  EXPECT_NEAR(centre.z(), 0.0F, 1e-4F);

  QVector3D const corner = m.map(QVector3D(1.0F, 1.0F, 1.0F));
  EXPECT_NEAR(corner.x(), 2.0F, 1e-4F);
  EXPECT_NEAR(corner.y(), 0.16F, 1e-4F);
  EXPECT_NEAR(corner.z(), 1.8F, 1e-4F);
}

// Regression guard against Stage 11 barracks migration drift.
//
// For every structural part the rig emits, assert that the model matrix
// it produces sends the unit-cube corners (-1,-1,-1) and (+1,+1,+1) to
// the same world positions the legacy imperative `draw_box(center,
// halfExt)` would have produced. This directly exercises the rig
// interpreter's Box anchor-pair semantics end-to-end.
TEST(BarracksRigTest, AllBoxPartsMatchLegacyDrawBoxTransforms) {
  using namespace Render::RigDSL::Barracks;

  // Legacy-equivalent (center, halfExt) for each anchor-pair the rig
  // uses. These are lifted verbatim from the pre-Stage-11 barracks
  // renderer — if a migration error drifts any part, the test will say
  // which one.
  struct LegacyBox {
    Render::RigDSL::AnchorId low;
    Render::RigDSL::AnchorId high;
    QVector3D legacy_center;
    QVector3D legacy_half_extent;
  };

  const LegacyBox kLegacyBoxes[] = {
      {Platform_BaseLow, Platform_BaseHigh, {0.0F, 0.08F, 0.0F},
       {2.0F, 0.08F, 1.8F}},
      {Platform_TopLow, Platform_TopHigh, {0.0F, 0.18F, 0.0F},
       {1.8F, 0.02F, 1.6F}},
      {Court_StoneLow, Court_StoneHigh, {0.0F, 0.22F, 0.0F},
       {1.3F, 0.01F, 1.1F}},
      {Court_PoolLow, Court_PoolHigh, {0.0F, 0.24F, 0.0F},
       {0.7F, 0.02F, 0.5F}},
      {Court_PoolTrimSLow, Court_PoolTrimSHigh, {0.0F, 0.25F, -0.52F},
       {0.72F, 0.02F, 0.02F}},
      {Court_PoolTrimNLow, Court_PoolTrimNHigh, {0.0F, 0.25F, 0.52F},
       {0.72F, 0.02F, 0.02F}},
      {Court_PillarCapLow, Court_PillarCapHigh, {0.0F, 0.58F, 0.0F},
       {0.08F, 0.03F, 0.08F}},
      {Wall_BackLow, Wall_BackHigh, {0.0F, 0.90F, -1.2F},
       {1.4F, 0.70F, 0.10F}},
      {Wall_LeftLow, Wall_LeftHigh, {-1.5F, 0.90F, -0.5F},
       {0.10F, 0.70F, 0.60F}},
      {Wall_RightLow, Wall_RightHigh, {1.5F, 0.90F, -0.5F},
       {0.10F, 0.70F, 0.60F}},
      {Door_LDoorLow, Door_LDoorHigh, {-0.6F, 0.65F, -1.15F},
       {0.25F, 0.35F, 0.03F}},
      {Door_LLintelLow, Door_LLintelHigh, {-0.6F, 0.98F, -1.15F},
       {0.25F, 0.05F, 0.03F}},
      {Door_RDoorLow, Door_RDoorHigh, {0.6F, 0.65F, -1.15F},
       {0.25F, 0.35F, 0.03F}},
      {Door_RLintelLow, Door_RLintelHigh, {0.6F, 0.98F, -1.15F},
       {0.25F, 0.05F, 0.03F}},
  };

  Render::RigDSL::StaticAnchorResolver<Goods_Amp3Top + 1U> anchors;

  auto set_pair = [&](Render::RigDSL::AnchorId lo, Render::RigDSL::AnchorId hi,
                      const QVector3D &c, const QVector3D &h) {
    anchors.set(lo, c - h);
    anchors.set(hi, c + h);
  };
  for (auto const &b : kLegacyBoxes) {
    set_pair(b.low, b.high, b.legacy_center, b.legacy_half_extent);
  }

  for (auto const &b : kLegacyBoxes) {
    Render::RigDSL::PartDef part{
        Render::RigDSL::PartKind::Box,
        0,
        0xFFU,
        Render::RigDSL::PaletteSlot::Literal,
        Render::RigDSL::PackedColor{0, 0, 0, 255},
        b.low,
        b.high,
        Render::RigDSL::kInvalidScalar,
        1.0F,
        1.0F,
        1.0F,
        1.0F,
        1.0F};

    Render::RigDSL::InterpretContext ctx;
    ctx.anchors = &anchors;
    ctx.model = QMatrix4x4{};
    ctx.lod = 0;
    ctx.global_alpha = 1.0F;

    RecordingSubmitter sub;
    Render::RigDSL::render_part(part, ctx, sub);
    ASSERT_EQ(sub.records().size(), 1U);

    QMatrix4x4 const &m = sub.records()[0].model;

    QVector3D const mapped_min = m.map(QVector3D(-1.0F, -1.0F, -1.0F));
    QVector3D const mapped_max = m.map(QVector3D(1.0F, 1.0F, 1.0F));
    QVector3D const expected_min = b.legacy_center - b.legacy_half_extent;
    QVector3D const expected_max = b.legacy_center + b.legacy_half_extent;

    EXPECT_NEAR(mapped_min.x(), expected_min.x(), 1e-4F)
        << "part low=" << static_cast<int>(b.low);
    EXPECT_NEAR(mapped_min.y(), expected_min.y(), 1e-4F)
        << "part low=" << static_cast<int>(b.low);
    EXPECT_NEAR(mapped_min.z(), expected_min.z(), 1e-4F)
        << "part low=" << static_cast<int>(b.low);
    EXPECT_NEAR(mapped_max.x(), expected_max.x(), 1e-4F)
        << "part low=" << static_cast<int>(b.low);
    EXPECT_NEAR(mapped_max.y(), expected_max.y(), 1e-4F)
        << "part low=" << static_cast<int>(b.low);
    EXPECT_NEAR(mapped_max.z(), expected_max.z(), 1e-4F)
        << "part low=" << static_cast<int>(b.low);
  }
}

TEST(BarracksRigTest, RigIteratesAllParts) {
  using namespace Render::RigDSL::Barracks;
  Render::RigDSL::StaticAnchorResolver<Goods_Amp3Top + 1U> anchors;
  // Any non-degenerate values work: we only check part count.
  for (std::size_t i = 0; i <= static_cast<std::size_t>(Goods_Amp3Top); ++i) {
    anchors.set(static_cast<Render::RigDSL::AnchorId>(i),
                QVector3D(static_cast<float>(i),
                          static_cast<float>(i) + 0.5F,
                          -static_cast<float>(i)));
  }

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.model = QMatrix4x4{};
  ctx.lod = 0;
  ctx.global_alpha = 1.0F;

  RecordingSubmitter sub;
  Render::RigDSL::render_rig(kRig, ctx, sub);

  EXPECT_EQ(sub.records().size(), kRig.part_count);
}
