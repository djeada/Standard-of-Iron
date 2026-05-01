

#include "render/equipment/equipment_submit.h"
#include "render/equipment/horse/armor/champion_renderer.h"
#include "render/equipment/horse/armor/crupper_renderer.h"
#include "render/equipment/horse/armor/leather_barding_renderer.h"
#include "render/equipment/horse/armor/scale_barding_renderer.h"
#include "render/equipment/horse/decorations/saddle_bag_renderer.h"
#include "render/equipment/horse/horse_attachment_archetype.h"
#include "render/equipment/horse/saddles/roman_saddle_renderer.h"
#include "render/equipment/horse/tack/blanket_renderer.h"
#include "render/horse/attachment_frames.h"
#include "render/horse/horse_spec.h"
#include "render/render_archetype.h"
#include "render/rigged_mesh_bake.h"
#include "render/static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <limits>

namespace {

struct AABB {
  QVector3D mn{std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity()};
  QVector3D mx{-std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity()};

  void include(const QVector3D &p) {
    mn.setX(std::min(mn.x(), p.x()));
    mn.setY(std::min(mn.y(), p.y()));
    mn.setZ(std::min(mn.z(), p.z()));
    mx.setX(std::max(mx.x(), p.x()));
    mx.setY(std::max(mx.y(), p.y()));
    mx.setZ(std::max(mx.z(), p.z()));
  }
  [[nodiscard]] auto valid() const noexcept -> bool {
    return mx.x() >= mn.x() && mx.y() >= mn.y() && mx.z() >= mn.z();
  }
};

auto legacy_archetype_aabb(const Render::GL::EquipmentBatch &batch) -> AABB {
  AABB box;
  for (const auto &prim : batch.archetypes) {
    if (prim.archetype == nullptr) {
      continue;
    }
    const auto &slice = prim.archetype->lods[0];
    for (const auto &draw : slice.draws) {
      const auto *mesh = draw.mesh;
      if (mesh == nullptr) {
        continue;
      }
      const QMatrix4x4 world = prim.world * draw.local_model;
      for (const auto &v : mesh->get_vertices()) {
        const QVector3D p{v.position[0], v.position[1], v.position[2]};
        box.include(world.map(p));
      }
    }
  }
  return box;
}

void expect_aabb_close(const AABB &a, const AABB &b, float eps) {
  ASSERT_TRUE(a.valid()) << "legacy AABB is empty";
  ASSERT_TRUE(b.valid()) << "baked AABB is empty";
  EXPECT_NEAR(a.mn.x(), b.mn.x(), eps);
  EXPECT_NEAR(a.mn.y(), b.mn.y(), eps);
  EXPECT_NEAR(a.mn.z(), b.mn.z(), eps);
  EXPECT_NEAR(a.mx.x(), b.mx.x(), eps);
  EXPECT_NEAR(a.mx.y(), b.mx.y(), eps);
  EXPECT_NEAR(a.mx.z(), b.mx.z(), eps);
}

auto bone_from_frame(const Render::GL::HorseAttachmentFrame &f) -> QMatrix4x4 {
  QMatrix4x4 m;
  m.setColumn(0, QVector4D(f.right, 0.0F));
  m.setColumn(1, QVector4D(f.up, 0.0F));
  m.setColumn(2, QVector4D(f.forward, 0.0F));
  m.setColumn(3, QVector4D(f.origin, 1.0F));
  return m;
}

auto bake_and_compare_single(const Render::GL::EquipmentBatch &legacy_batch,
                             const Render::Creature::StaticAttachmentSpec &spec,
                             const QMatrix4x4 &bone_world) -> void {
  std::array<Render::Creature::BoneWorldMatrix, 1> bind{bone_world};
  std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{spec};

  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB legacy = legacy_archetype_aabb(legacy_batch);

  AABB bake;
  for (const auto &v : baked.vertices) {
    const QVector3D local{v.position_bone_local[0], v.position_bone_local[1],
                          v.position_bone_local[2]};
    bake.include(bone_world.map(local));
  }

  expect_aabb_close(legacy, bake, 1e-3F);
}

auto make_test_back_center_frame() -> Render::GL::HorseAttachmentFrame {
  Render::GL::HorseAttachmentFrame f;
  f.origin = QVector3D(0.0F, 1.05F, -0.04F);
  f.right = QVector3D(1.0F, 0.0F, 0.0F);
  f.up = QVector3D(0.0F, 1.0F, 0.0F);
  f.forward = QVector3D(0.0F, 0.0F, 1.0F);
  return f;
}

auto make_test_chest_frame() -> Render::GL::HorseAttachmentFrame {
  Render::GL::HorseAttachmentFrame f;
  f.origin = QVector3D(0.0F, 1.10F, 0.30F);
  f.right = QVector3D(1.0F, 0.0F, 0.0F);
  f.up = QVector3D(0.0F, 1.0F, 0.0F);
  f.forward = QVector3D(0.0F, 0.0F, 1.0F);
  return f;
}

auto make_test_barrel_frame() -> Render::GL::HorseAttachmentFrame {
  Render::GL::HorseAttachmentFrame f;
  f.origin = QVector3D(0.0F, 0.95F, 0.0F);
  f.right = QVector3D(1.0F, 0.0F, 0.0F);
  f.up = QVector3D(0.0F, 1.0F, 0.0F);
  f.forward = QVector3D(0.0F, 0.0F, 1.0F);
  return f;
}

auto make_test_rump_frame() -> Render::GL::HorseAttachmentFrame {
  Render::GL::HorseAttachmentFrame f;
  f.origin = QVector3D(0.0F, 1.00F, -0.38F);
  f.right = QVector3D(1.0F, 0.0F, 0.0F);
  f.up = QVector3D(0.0F, 1.0F, 0.0F);
  f.forward = QVector3D(0.0F, 0.0F, 1.0F);
  return f;
}

TEST(BakedHorseAttachmentWorldPosition, RomanSaddleRootBoneBakeIsValid) {
  using Render::Horse::HorseBone;

  auto bind = Render::Horse::horse_bind_palette();
  ASSERT_GE(bind.size(), Render::Horse::k_horse_bone_count);

  Render::GL::HorseAttachmentFrame frame;
  frame.origin = QVector3D(0.0F, 1.05F, -0.04F);
  frame.right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.forward = QVector3D(0.0F, 0.0F, 1.0F);

  const auto root_bind = bind[static_cast<std::size_t>(HorseBone::Root)];
  const auto spec = Render::GL::roman_saddle_make_static_attachment(
      static_cast<std::uint16_t>(HorseBone::Root), 9U, frame, root_bind);

  std::vector<Render::Creature::BoneWorldMatrix> bind_vec(bind.begin(),
                                                          bind.end());
  std::array<Render::Creature::StaticAttachmentSpec, 1> attachments{spec};

  Render::Creature::BakeInput input{};
  input.bind_pose = bind_vec;
  input.attachments = attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());
  for (const auto &v : baked.vertices) {
    EXPECT_TRUE(std::isfinite(v.position_bone_local[0]));
    EXPECT_TRUE(std::isfinite(v.position_bone_local[1]));
    EXPECT_TRUE(std::isfinite(v.position_bone_local[2]));
  }
}

TEST(BakedHorseAttachmentWorldPosition, BlanketMatchesLegacySubmit) {
  const auto frame = make_test_back_center_frame();
  const QMatrix4x4 bone_world = bone_from_frame(frame);

  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::HorseBodyFrames frames{};
  frames.back_center = frame;
  Render::GL::HorseVariant variant{};
  variant.blanket_color = QVector3D(0.20F, 0.50F, 0.80F);
  Render::GL::HorseAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::BlanketRenderer::submit(ctx, frames, variant, anim, legacy_batch);
  ASSERT_FALSE(legacy_batch.archetypes.empty());

  const auto spec =
      Render::GL::blanket_make_static_attachment(0, 9U, frame, bone_world);
  bake_and_compare_single(legacy_batch, spec, bone_world);
}

TEST(BakedHorseAttachmentWorldPosition, CrupperMatchesLegacySubmit) {
  const auto frame = make_test_rump_frame();
  const QMatrix4x4 bone_world = bone_from_frame(frame);

  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::HorseBodyFrames frames{};
  frames.rump = frame;
  Render::GL::HorseVariant variant{};
  variant.tack_color = QVector3D(0.60F, 0.40F, 0.20F);
  Render::GL::HorseAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::CrupperRenderer::submit(ctx, frames, variant, anim, legacy_batch);
  ASSERT_FALSE(legacy_batch.archetypes.empty());

  const auto spec =
      Render::GL::crupper_make_static_attachment(0, 9U, frame, bone_world);
  bake_and_compare_single(legacy_batch, spec, bone_world);
}

TEST(BakedHorseAttachmentWorldPosition, ChampionMatchesLegacySubmit) {
  const auto frame = make_test_chest_frame();
  const QMatrix4x4 bone_world = bone_from_frame(frame);

  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::HorseBodyFrames frames{};
  frames.chest = frame;
  Render::GL::HorseVariant variant{};
  variant.tack_color = QVector3D(0.72F, 0.62F, 0.44F);
  Render::GL::HorseAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::ChampionRenderer::submit(ctx, frames, variant, anim,
                                       legacy_batch);
  ASSERT_FALSE(legacy_batch.archetypes.empty());

  const auto spec =
      Render::GL::champion_make_static_attachment(0, 9U, frame, bone_world);
  bake_and_compare_single(legacy_batch, spec, bone_world);
}

TEST(BakedHorseAttachmentWorldPosition, SaddleBagMatchesLegacySubmit) {
  const auto frame = make_test_back_center_frame();
  const QMatrix4x4 bone_world = bone_from_frame(frame);

  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::HorseBodyFrames frames{};
  frames.back_center = frame;
  Render::GL::HorseVariant variant{};
  variant.saddle_color = QVector3D(0.42F, 0.28F, 0.14F);
  variant.tack_color = QVector3D(0.30F, 0.20F, 0.10F);
  Render::GL::HorseAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::SaddleBagRenderer::submit(ctx, frames, variant, anim,
                                        legacy_batch);
  ASSERT_FALSE(legacy_batch.archetypes.empty());

  const auto spec =
      Render::GL::saddle_bag_make_static_attachment(0, 9U, frame, bone_world);
  bake_and_compare_single(legacy_batch, spec, bone_world);
}

TEST(BakedHorseAttachmentWorldPosition, ScaleBardingChestMatchesLegacySubmit) {
  const auto chest_frame = make_test_chest_frame();
  const QMatrix4x4 bone_world = bone_from_frame(chest_frame);

  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};

  Render::GL::HorseBodyFrames frames{};
  frames.chest = chest_frame;

  Render::GL::HorseVariant variant{};
  variant.tack_color = QVector3D(0.55F, 0.45F, 0.25F);
  Render::GL::HorseAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::append_horse_attachment_archetype(
      legacy_batch, ctx, chest_frame,
      Render::GL::scale_barding_chest_archetype(),
      std::array<QVector3D, 1>{variant.tack_color * 0.85F});
  ASSERT_FALSE(legacy_batch.archetypes.empty());

  const auto spec = Render::GL::scale_barding_make_static_attachment(
      Render::GL::scale_barding_chest_archetype(), 0, 9U, chest_frame,
      bone_world);
  bake_and_compare_single(legacy_batch, spec, bone_world);
}

TEST(BakedHorseAttachmentWorldPosition, ScaleBardingBarrelMatchesLegacySubmit) {
  const auto barrel_frame = make_test_barrel_frame();
  const QMatrix4x4 bone_world = bone_from_frame(barrel_frame);

  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::HorseVariant variant{};
  variant.tack_color = QVector3D(0.55F, 0.45F, 0.25F);
  Render::GL::HorseAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::append_horse_attachment_archetype(
      legacy_batch, ctx, barrel_frame,
      Render::GL::scale_barding_barrel_archetype(),
      std::array<QVector3D, 1>{variant.tack_color * 0.85F});
  ASSERT_FALSE(legacy_batch.archetypes.empty());

  const auto spec = Render::GL::scale_barding_make_static_attachment(
      Render::GL::scale_barding_barrel_archetype(), 0, 9U, barrel_frame,
      bone_world);
  bake_and_compare_single(legacy_batch, spec, bone_world);
}

TEST(BakedHorseAttachmentWorldPosition,
     LeatherBardingChestMatchesLegacySubmit) {
  const auto chest_frame = make_test_chest_frame();
  const QMatrix4x4 bone_world = bone_from_frame(chest_frame);

  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::HorseVariant variant{};
  variant.saddle_color = QVector3D(0.45F, 0.32F, 0.18F);
  Render::GL::HorseAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::append_horse_attachment_archetype(
      legacy_batch, ctx, chest_frame,
      Render::GL::leather_barding_chest_archetype(),
      std::array<QVector3D, 1>{variant.saddle_color * 0.90F});
  ASSERT_FALSE(legacy_batch.archetypes.empty());

  const auto spec = Render::GL::leather_barding_make_static_attachment(
      Render::GL::leather_barding_chest_archetype(), 0, 9U, chest_frame,
      bone_world);
  bake_and_compare_single(legacy_batch, spec, bone_world);
}

} // namespace
