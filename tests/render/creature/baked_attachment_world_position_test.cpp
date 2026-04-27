// Verifies that the bake path produces world-space attachment vertex
// positions equivalent (within ε) to the legacy IEquipmentRenderer
// submit path. This is the geometric ground-truth check that
// p5-merged-archetype-mesh's per-renderer migration relies on:
//
//   bone_world * inverse_bind * baked_vertex   ==   T(origin + R*lo*r) *
//   basis(R*r,U*r,F*r) * draw.local_model * v
//
// At bind pose, bone_world * inverse_bind = identity, so the baked
// vertex IS the legacy world position. The conversion rule lives in
// `make_static_attachment_spec_from_humanoid_renderer` in
// render/equipment/humanoid_attachment_archetype.h.

#include "render/equipment/armor/roman_greaves.h"
#include "render/equipment/armor/roman_shoulder_cover.h"
#include "render/equipment/armor/shoulder_cover_archetype.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/helmets/carthage_light_helmet.h"
#include "render/equipment/humanoid_attachment_archetype.h"
#include "render/equipment/weapons/roman_scutum.h"
#include "render/equipment/weapons/shield_carthage.h"
#include "render/equipment/weapons/shield_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/render_archetype.h"
#include "render/rigged_mesh_bake.h"
#include "render/static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
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
  [[nodiscard]] auto extents() const noexcept -> QVector3D { return mx - mn; }
  [[nodiscard]] auto center() const noexcept -> QVector3D {
    return (mn + mx) * 0.5F;
  }
};

auto bone_matrix_from_frame(const Render::GL::AttachmentFrame &f)
    -> QMatrix4x4 {
  // The legacy frame's basis (right/up/forward) is the bone's local
  // basis in model space, with origin at the joint head. This matches
  // BoneBasisKind::FromParent for the head bone (bind pose: same
  // basis as Neck, origin at head_pos).
  QMatrix4x4 m;
  m.setColumn(0, QVector4D(f.right, 0.0F));
  m.setColumn(1, QVector4D(f.up, 0.0F));
  m.setColumn(2, QVector4D(f.forward, 0.0F));
  m.setColumn(3, QVector4D(f.origin, 1.0F));
  return m;
}

// Walk a legacy EquipmentBatch's archetype prims and accumulate
// world-space vertex positions into an AABB. Each prim represents one
// archetype instance: world = prim.world * draw.local_model * v.position.
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

auto baked_aabb(const Render::Creature::BakedRiggedMeshCpu &baked) -> AABB {
  AABB box;
  for (const auto &v : baked.vertices) {
    box.include(QVector3D(v.position_bone_local[0], v.position_bone_local[1],
                          v.position_bone_local[2]));
  }
  return box;
}

auto baked_skinned_aabb(
    const Render::Creature::BakedRiggedMeshCpu &baked,
    std::span<const Render::Creature::BoneWorldMatrix> bind_pose) -> AABB {
  AABB box;
  for (const auto &v : baked.vertices) {
    QVector3D pos{v.position_bone_local[0], v.position_bone_local[1],
                  v.position_bone_local[2]};
    const auto bone = static_cast<std::size_t>(v.bone_indices[0]);
    if (bone < bind_pose.size()) {
      pos = bind_pose[bone].map(pos);
    }
    box.include(pos);
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

void expect_aabb_extent_close(const AABB &a, const AABB &b, float eps) {
  ASSERT_TRUE(a.valid()) << "legacy AABB is empty";
  ASSERT_TRUE(b.valid()) << "baked AABB is empty";
  const QVector3D ae = a.extents();
  const QVector3D be = b.extents();
  EXPECT_NEAR(ae.x(), be.x(), eps);
  EXPECT_NEAR(ae.y(), be.y(), eps);
  EXPECT_NEAR(ae.z(), be.z(), eps);
}

// Constructs an AttachmentFrame and matching bone matrix that the
// renderer would see at runtime. We use a non-trivial origin so any
// translation drift shows up.
auto make_test_head_frame() -> Render::GL::AttachmentFrame {
  Render::GL::AttachmentFrame f;
  f.origin = QVector3D(0.0F, 1.7F, 0.0F);
  f.right = QVector3D(1.0F, 0.0F, 0.0F);
  f.up = QVector3D(0.0F, 1.0F, 0.0F);
  f.forward = QVector3D(0.0F, 0.0F, 1.0F);
  f.radius = 0.16F;
  return f;
}

TEST(BakedAttachmentWorldPosition, CarthageLightHelmetMatchesLegacySubmit) {
  const auto frame = make_test_head_frame();
  const QMatrix4x4 bone_world = bone_matrix_from_frame(frame);

  // ---- Legacy path ----
  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{}; // identity — we want raw world output
  Render::GL::BodyFrames frames{};
  frames.head = frame;
  Render::GL::HumanoidPalette palette{};
  Render::GL::HumanoidAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 8);
  Render::GL::CarthageLightHelmetRenderer::submit(
      Render::GL::CarthageLightHelmetConfig{}, ctx, frames, palette, anim,
      legacy_batch);
  ASSERT_FALSE(legacy_batch.archetypes.empty())
      << "renderer must emit at least one archetype prim";

  const AABB legacy = legacy_archetype_aabb(legacy_batch);

  // ---- Bake path ----
  // Bake all 6 sub-archetypes (default config: crest=true, nasal=true)
  // together, then compare the combined AABB to the legacy AABB.
  constexpr std::uint16_t k_head_bone = 0;
  constexpr std::uint8_t k_base_role = 0;
  const std::array<Render::Creature::StaticAttachmentSpec, 6> k_attachments{
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_shell_archetype(), k_head_bone,
          k_base_role, bone_world),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_neck_guard_archetype(), k_head_bone,
          k_base_role, bone_world),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_cheek_guards_archetype(),
          k_head_bone, k_base_role, bone_world),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_nasal_guard_archetype(),
          k_head_bone, k_base_role, bone_world),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_crest_archetype(
              /*high_detail=*/true),
          k_head_bone, k_base_role, bone_world),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_rivets_archetype(), k_head_bone,
          k_base_role, bone_world),
  };

  std::array<Render::Creature::BoneWorldMatrix, 1> bind{bone_world};

  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = k_attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB bake = baked_aabb(baked);

  // 1mm precision is plenty for a unit ~30cm helmet.
  expect_aabb_close(legacy, bake, 1e-3F);
}

TEST(BakedAttachmentWorldPosition, RomanGreavesMatchesLegacySubmit) {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const auto shin_frame = bind_frames.shin_l;
  const QMatrix4x4 bone_world = bone_matrix_from_frame(shin_frame);

  // ---- Legacy path (only render shin_l; zero out shin_r) ----
  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::BodyFrames frames{};
  frames.shin_l = shin_frame;
  Render::GL::AttachmentFrame empty_shin{};
  empty_shin.radius = 0.0F;
  frames.shin_r = empty_shin;
  Render::GL::HumanoidPalette palette{};
  Render::GL::HumanoidAnimationContext anim{};

  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.reserve(0, 0, 4);
  Render::GL::RomanGreavesRenderer::submit(Render::GL::RomanGreavesConfig{},
                                           ctx, frames, palette, anim,
                                           legacy_batch);
  ASSERT_FALSE(legacy_batch.archetypes.empty())
      << "renderer must emit at least one archetype prim";
  const AABB legacy = legacy_archetype_aabb(legacy_batch);

  // ---- Bake path ----
  constexpr std::uint16_t k_bone = 0;
  constexpr std::uint8_t k_base_role = 0;
  const std::array<Render::Creature::StaticAttachmentSpec, 1> k_attachments{
      Render::GL::roman_greaves_make_static_attachment(k_bone, k_base_role,
                                                       bone_world),
  };
  std::array<Render::Creature::BoneWorldMatrix, 1> bind{bone_world};

  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = k_attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB bake = baked_aabb(baked);
  expect_aabb_close(legacy, bake, 1e-3F);
}

TEST(BakedAttachmentWorldPosition, RomanShoulderCoverMatchesLegacySubmit) {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const QMatrix4x4 bone_world = Render::GL::make_shoulder_cover_transform(
      QMatrix4x4{}, bind_frames.shoulder_l.origin, -bind_frames.torso.right,
      bind_frames.torso.up);

  // ---- Legacy path (zero shoulder_r so only shoulder_l renders) ----
  // We can't easily skip the right shoulder since the renderer always
  // submits both. Instead we render only the left side by computing the
  // legacy AABB using a frames struct where shoulder_r = shoulder_l (both
  // produce the same archetype but with different transforms). To compare
  // bake (single attachment) to a single legacy emission, we drive the
  // renderer with a custom batch that we filter to only the shoulder_l
  // archetype. The shoulder_l archetype is the first emitted.
  Render::GL::DrawContext ctx;
  ctx.model = QMatrix4x4{};
  Render::GL::BodyFrames frames{};
  frames.torso = bind_frames.torso;
  frames.shoulder_l = bind_frames.shoulder_l;
  frames.shoulder_r = bind_frames.shoulder_r;
  Render::GL::HumanoidPalette palette{};
  Render::GL::HumanoidAnimationContext anim{};

  Render::GL::EquipmentBatch full_batch;
  full_batch.reserve(0, 0, 4);
  Render::GL::RomanShoulderCoverRenderer::submit(
      Render::GL::RomanShoulderCoverConfig{}, ctx, frames, palette, anim,
      full_batch);
  ASSERT_GE(full_batch.archetypes.size(), 1U);

  // Take only the first archetype prim (shoulder_l) for comparison.
  Render::GL::EquipmentBatch legacy_batch;
  legacy_batch.archetypes.push_back(full_batch.archetypes[0]);
  const AABB legacy = legacy_archetype_aabb(legacy_batch);

  // ---- Bake path ----
  constexpr std::uint16_t k_bone = 0;
  constexpr std::uint8_t k_base_role = 0;
  const std::array<Render::Creature::StaticAttachmentSpec, 1> k_attachments{
      Render::GL::roman_shoulder_cover_make_static_attachment(
          k_bone, k_base_role, bone_world),
  };
  std::array<Render::Creature::BoneWorldMatrix, 1> bind{bone_world};

  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = k_attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB bake = baked_aabb(baked);
  expect_aabb_close(legacy, bake, 1e-3F);
}

TEST(BakedAttachmentWorldPosition, GenericShieldUsesGripSocketPose) {
  const auto &bind_palette = Render::Humanoid::humanoid_bind_palette();

  Render::GL::ShieldRenderConfig shield_cfg;
  shield_cfg.shield_radius = 0.18F;
  shield_cfg.shield_aspect = 1.3F;
  shield_cfg.has_cross_decal = false;
  constexpr std::uint8_t k_base_role = 0;
  const std::array<Render::Creature::StaticAttachmentSpec, 1> k_attachments{
      Render::GL::shield_make_static_attachment(shield_cfg, k_base_role),
  };
  std::array<Render::Creature::BoneWorldMatrix, Render::Humanoid::kBoneCount>
      bind{};
  std::copy(bind_palette.begin(), bind_palette.end(), bind.begin());
  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = k_attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB baked_world = baked_skinned_aabb(
      baked, std::span<const Render::Creature::BoneWorldMatrix>(bind.data(),
                                                                bind.size()));
  EXPECT_GT(baked_world.extents().y(), baked_world.extents().x() * 2.0F);
  EXPECT_GT(baked_world.extents().z(), baked_world.extents().x() * 1.5F);
}

TEST(BakedAttachmentWorldPosition, RomanScutumUsesGripSocketPose) {
  const auto &bind_palette = Render::Humanoid::humanoid_bind_palette();

  constexpr std::uint8_t k_base_role = 0;
  const std::array<Render::Creature::StaticAttachmentSpec, 1> k_attachments{
      Render::GL::roman_scutum_make_static_attachment(k_base_role),
  };
  std::array<Render::Creature::BoneWorldMatrix, Render::Humanoid::kBoneCount>
      bind{};
  std::copy(bind_palette.begin(), bind_palette.end(), bind.begin());
  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = k_attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB baked_world = baked_skinned_aabb(
      baked, std::span<const Render::Creature::BoneWorldMatrix>(bind.data(),
                                                                bind.size()));
  EXPECT_GT(baked_world.extents().y(), baked_world.extents().x() * 2.0F);
  EXPECT_GT(baked_world.extents().z(), baked_world.extents().x() * 1.5F);
}

TEST(BakedAttachmentWorldPosition, CarthageShieldUsesGripSocketPose) {
  const auto &bind_palette = Render::Humanoid::humanoid_bind_palette();

  constexpr std::uint8_t k_base_role = 0;
  const std::array<Render::Creature::StaticAttachmentSpec, 1> k_attachments{
      Render::GL::carthage_shield_make_static_attachment(
          Render::GL::CarthageShieldConfig{}, k_base_role),
  };
  std::array<Render::Creature::BoneWorldMatrix, Render::Humanoid::kBoneCount>
      bind{};
  std::copy(bind_palette.begin(), bind_palette.end(), bind.begin());
  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = k_attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB baked_world = baked_skinned_aabb(
      baked, std::span<const Render::Creature::BoneWorldMatrix>(bind.data(),
                                                                bind.size()));
  EXPECT_GT(baked_world.extents().y(), baked_world.extents().x());
  EXPECT_GT(baked_world.extents().z(), baked_world.extents().x() * 0.5F);
}

TEST(BakedAttachmentWorldPosition, GenericSwordUsesGripSocketPose) {
  const auto &bind_palette = Render::Humanoid::humanoid_bind_palette();

  Render::GL::SwordRenderConfig sword_cfg;
  sword_cfg.sword_length = 0.80F;
  sword_cfg.sword_width = 0.060F;
  sword_cfg.guard_half_width = 0.120F;
  sword_cfg.handle_radius = 0.016F;
  sword_cfg.pommel_radius = 0.045F;
  sword_cfg.blade_ricasso = 0.14F;
  sword_cfg.material_id = 3;
  constexpr std::uint8_t k_base_role = 0;
  const std::array<Render::Creature::StaticAttachmentSpec, 1> k_attachments{
      Render::GL::sword_make_static_attachment(sword_cfg, k_base_role),
  };
  std::array<Render::Creature::BoneWorldMatrix, Render::Humanoid::kBoneCount>
      bind{};
  std::copy(bind_palette.begin(), bind_palette.end(), bind.begin());
  Render::Creature::BakeInput input{};
  input.bind_pose = bind;
  input.attachments = k_attachments;
  const auto baked = Render::Creature::bake_rigged_mesh_cpu(input);
  ASSERT_FALSE(baked.vertices.empty());

  const AABB baked_world = baked_skinned_aabb(
      baked, std::span<const Render::Creature::BoneWorldMatrix>(bind.data(),
                                                                bind.size()));
  EXPECT_GT(baked_world.extents().y(), baked_world.extents().z() * 3.0F);
  EXPECT_GT(baked_world.extents().y(), baked_world.extents().x());
}

} // namespace
