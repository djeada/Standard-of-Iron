// Stage 8a — Rig DSL interpreter tests.
//
// These tests lock in the DSL's core contract:
//   1. A RigDef with N parts emits exactly N draw commands (one per part
//      that passes its LOD mask).
//   2. Anchor IDs are resolved through the pluggable AnchorResolver.
//   3. Palette slots are resolved through the pluggable PaletteResolver,
//      and Literal slots use the packed colour directly.
//   4. LOD masks skip parts cheaply without touching the anchor resolver.
//   5. With a Material in the context, emission routes through
//      ISubmitter::part() -> DrawPartCmd (v2 path). Without one, it falls
//      back to MeshCmd.

#include "render/draw_queue.h"
#include "render/material.h"
#include "render/rig_dsl/defs/watchtower_rig.h"
#include "render/rig_dsl/rig_def.h"
#include "render/rig_dsl/rig_interpreter.h"
#include "render/submitter.h"

#include <gtest/gtest.h>

#include <array>
#include <unordered_map>

namespace {

// Minimal AnchorResolver backed by a flat map. Real rigs plug in their
// species-specific bone lookup here.
class FlatAnchors : public Render::RigDSL::AnchorResolver {
public:
  void set(Render::RigDSL::AnchorId id, const QVector3D &pos) {
    m_map[id] = pos;
  }
  [[nodiscard]] auto
  resolve(Render::RigDSL::AnchorId id) const -> QVector3D override {
    ++m_resolve_count;
    auto it = m_map.find(id);
    return it == m_map.end() ? QVector3D{} : it->second;
  }
  [[nodiscard]] auto resolve_count() const -> int { return m_resolve_count; }

private:
  std::unordered_map<Render::RigDSL::AnchorId, QVector3D> m_map;
  mutable int m_resolve_count{0};
};

class ConstantPalette : public Render::RigDSL::PaletteResolver {
public:
  [[nodiscard]] auto
  resolve(Render::RigDSL::PaletteSlot /*slot*/) const -> QVector3D override {
    return {0.1F, 0.2F, 0.3F};
  }
};

// Scalar resolver backed by a flat array. Lets tests verify that the
// per-part scale_id is honoured by the interpreter.
class FlatScalars : public Render::RigDSL::ScalarResolver {
public:
  void set(Render::RigDSL::ScalarId id, float v) { m_map[id] = v; }
  [[nodiscard]] auto
  resolve(Render::RigDSL::ScalarId id) const -> float override {
    auto it = m_map.find(id);
    return it == m_map.end() ? 1.0F : it->second;
  }

private:
  std::unordered_map<Render::RigDSL::ScalarId, float> m_map;
};

} // namespace

TEST(RigDSL, WatchtowerEmitsOnePartPerDef) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);

  FlatAnchors anchors;
  for (Render::RigDSL::AnchorId id = 0;
       id <= Render::RigDSL::Watchtower::Roof_Apex; ++id) {
    anchors.set(id, QVector3D(static_cast<float>(id) * 0.1F, 1.0F, 0.0F));
  }
  ConstantPalette palette;

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.palette = &palette;
  ctx.lod = 0;

  Render::RigDSL::render_rig(Render::RigDSL::Watchtower::kRig, ctx, submitter);

  EXPECT_EQ(queue.size(), Render::RigDSL::Watchtower::kRig.part_count);
}

TEST(RigDSL, SphereOnlyConsultsAnchorAOnce) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);
  FlatAnchors anchors;
  anchors.set(0, QVector3D(1.0F, 2.0F, 3.0F));
  ConstantPalette palette;

  constexpr Render::RigDSL::PartDef sphere_part{
      Render::RigDSL::PartKind::Sphere,
      0, 0xFFU, Render::RigDSL::PaletteSlot::Literal, {255, 0, 0, 255},
      0, Render::RigDSL::kInvalidAnchor, Render::RigDSL::kInvalidScalar, 0.5F, 1.0F, 1.0F, 1.0F, 1.0F};
  constexpr Render::RigDSL::PartDef arr[] = {sphere_part};
  auto const rig = Render::RigDSL::make_rig("s", arr);

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.palette = &palette;
  Render::RigDSL::render_rig(rig, ctx, submitter);

  EXPECT_EQ(queue.size(), 1U);
  EXPECT_EQ(anchors.resolve_count(), 1);
}

TEST(RigDSL, LodMaskSkipsPartsWithoutResolvingAnchors) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);
  FlatAnchors anchors;
  ConstantPalette palette;

  // Only bit 0 set → part visible at LOD 0, skipped at LOD 1/2/....
  constexpr Render::RigDSL::PartDef p{
      Render::RigDSL::PartKind::Cylinder,
      0, 0x01U, Render::RigDSL::PaletteSlot::Literal, {255, 255, 255, 255},
      0, 1, Render::RigDSL::kInvalidScalar, 0.1F, 1.0F, 1.0F, 1.0F, 1.0F};
  constexpr Render::RigDSL::PartDef arr[] = {p};
  auto const rig = Render::RigDSL::make_rig("lod", arr);

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.palette = &palette;

  ctx.lod = 1; // Not in mask.
  Render::RigDSL::render_rig(rig, ctx, submitter);
  EXPECT_EQ(queue.size(), 0U);
  EXPECT_EQ(anchors.resolve_count(), 0)
      << "LOD-skipped parts must not touch the resolver";

  ctx.lod = 0; // In mask.
  Render::RigDSL::render_rig(rig, ctx, submitter);
  EXPECT_EQ(queue.size(), 1U);
}

TEST(RigDSL, MaterialContextRoutesThroughDrawPartCmd) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);
  FlatAnchors anchors;
  anchors.set(0, QVector3D());
  anchors.set(1, QVector3D(0, 1, 0));
  ConstantPalette palette;

  Render::GL::Material mat{};

  constexpr Render::RigDSL::PartDef p{
      Render::RigDSL::PartKind::Cylinder,
      0, 0xFFU, Render::RigDSL::PaletteSlot::Literal, {255, 255, 255, 255},
      0, 1, Render::RigDSL::kInvalidScalar, 0.1F, 1.0F, 1.0F, 1.0F, 1.0F};
  constexpr Render::RigDSL::PartDef arr[] = {p};
  auto const rig = Render::RigDSL::make_rig("m", arr);

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.palette = &palette;
  ctx.material = &mat;
  Render::RigDSL::render_rig(rig, ctx, submitter);

  ASSERT_EQ(queue.size(), 1U);
  EXPECT_EQ(queue.items().front().index(), Render::GL::DrawPartCmdIndex);
}

TEST(RigDSL, NoMaterialContextFallsBackToMeshCmd) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);
  FlatAnchors anchors;
  anchors.set(0, QVector3D());
  anchors.set(1, QVector3D(0, 1, 0));
  ConstantPalette palette;

  constexpr Render::RigDSL::PartDef p{
      Render::RigDSL::PartKind::Cylinder,
      0, 0xFFU, Render::RigDSL::PaletteSlot::Literal, {255, 255, 255, 255},
      0, 1, Render::RigDSL::kInvalidScalar, 0.1F, 1.0F, 1.0F, 1.0F, 1.0F};
  constexpr Render::RigDSL::PartDef arr[] = {p};
  auto const rig = Render::RigDSL::make_rig("n", arr);

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.palette = &palette;
  // ctx.material intentionally left null.
  Render::RigDSL::render_rig(rig, ctx, submitter);

  ASSERT_EQ(queue.size(), 1U);
  auto const idx = queue.items().front().index();
  // Unit-cylinder emissions get auto-decomposed to CylinderCmd by the
  // submitter's fast-path, so either index is acceptable here.
  EXPECT_TRUE(idx == Render::GL::MeshCmdIndex ||
              idx == Render::GL::CylinderCmdIndex);
}

TEST(RigDSL, LiteralPaletteBypassesResolver) {
  // Even when a PaletteResolver is present, a Literal slot uses the packed
  // colour. Use a Sphere so we hit the straight MeshCmd path (no fast-path
  // decomposition) and can inspect the colour directly.
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);
  FlatAnchors anchors;
  anchors.set(0, QVector3D());
  ConstantPalette palette; // resolve() always returns (0.1, 0.2, 0.3)

  constexpr Render::RigDSL::PartDef p{
      Render::RigDSL::PartKind::Sphere,
      0, 0xFFU, Render::RigDSL::PaletteSlot::Literal, {200, 100, 50, 255},
      0, Render::RigDSL::kInvalidAnchor, Render::RigDSL::kInvalidScalar, 0.1F, 1.0F, 1.0F, 1.0F, 1.0F};
  constexpr Render::RigDSL::PartDef arr[] = {p};
  auto const rig = Render::RigDSL::make_rig("lit", arr);

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.palette = &palette;
  Render::RigDSL::render_rig(rig, ctx, submitter);

  ASSERT_EQ(queue.size(), 1U);
  auto const &cmd = std::get<Render::GL::MeshCmdIndex>(queue.items().front());
  // Literal 200/100/50 -> ~0.784 / 0.392 / 0.196.
  EXPECT_NEAR(cmd.color.x(), 200.0F / 255.0F, 1e-4F);
  EXPECT_NEAR(cmd.color.y(), 100.0F / 255.0F, 1e-4F);
  EXPECT_NEAR(cmd.color.z(), 50.0F / 255.0F, 1e-4F);
}

// Stage 8b — ScalarResolver multiplies the per-part size by the scale_id's
// current value. kInvalidScalar means "use size_x/y/z as-is".
TEST(RigDSL, ScalarResolverScalesSphereRadius) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);
  FlatAnchors anchors;
  anchors.set(0, QVector3D());
  FlatScalars scalars;
  scalars.set(7, 2.5F); // scale_id=7 -> radius multiplied by 2.5

  constexpr Render::RigDSL::PartDef p{
      Render::RigDSL::PartKind::Sphere,
      0, 0xFFU, Render::RigDSL::PaletteSlot::Literal, {255, 255, 255, 255},
      0, Render::RigDSL::kInvalidAnchor, /*scale_id=*/7, 0.2F, 1.0F, 1.0F,
      1.0F, 1.0F};
  constexpr Render::RigDSL::PartDef arr[] = {p};
  auto const rig = Render::RigDSL::make_rig("s", arr);

  Render::RigDSL::InterpretContext ctx;
  ctx.anchors = &anchors;
  ctx.scalars = &scalars;
  Render::RigDSL::render_rig(rig, ctx, submitter);

  ASSERT_EQ(queue.size(), 1U);
  auto const &cmd = std::get<Render::GL::MeshCmdIndex>(queue.items().front());
  // Effective radius = 0.2 * 2.5 = 0.5, so model column 0's length is 0.5.
  const QVector3D col0(cmd.model(0, 0), cmd.model(1, 0), cmd.model(2, 0));
  EXPECT_NEAR(col0.length(), 0.5F, 1e-4F);
}

