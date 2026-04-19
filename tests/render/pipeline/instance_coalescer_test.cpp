// Stage 9 — instance_coalescer tests.
//
// These tests lock the rules for fusing adjacent DrawPartCmds into a single
// InstancedPartBatch. The rules matter for correctness (transparent
// ordering, bone-palette exclusivity) and for perf (min-run threshold).

#include "render/draw_part.h"
#include "render/draw_queue.h" // k_opaque_threshold
#include "render/material.h"
#include "render/pipeline/instance_coalescer.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

using Render::GL::DrawPartCmd;
using Render::GL::Material;
using Render::GL::Mesh;
using Render::GL::Texture;

namespace {

// Fake pointers — the coalescer only compares pointer identity, it never
// dereferences the mesh / material / texture. This lets the tests run
// without GL context.
auto fake_mesh(std::uintptr_t tag) -> Mesh * {
  return reinterpret_cast<Mesh *>(tag);
}
auto fake_material(std::uintptr_t tag) -> const Material * {
  return reinterpret_cast<const Material *>(tag);
}
auto fake_texture(std::uintptr_t tag) -> Texture * {
  return reinterpret_cast<Texture *>(tag);
}

auto make_part(Mesh *m, const Material *mat, Texture *tex = nullptr,
               std::int32_t mid = 0, float alpha = 1.0F) -> DrawPartCmd {
  DrawPartCmd p;
  p.mesh = m;
  p.material = mat;
  p.texture = tex;
  p.material_id = mid;
  p.alpha = alpha;
  return p;
}

} // namespace

TEST(InstanceCoalescer, EmptyInputYieldsNoBatches) {
  std::array<DrawPartCmd, 0> parts;
  const auto batches = Render::Pipeline::coalesce_instances(parts);
  EXPECT_TRUE(batches.empty());
}

TEST(InstanceCoalescer, FusesFourIdenticalPartsIntoOneBatch) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  std::array<DrawPartCmd, 4> parts{make_part(m, mat), make_part(m, mat),
                                   make_part(m, mat), make_part(m, mat)};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 4);
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches[0].count, 4U);
  EXPECT_EQ(batches[0].instances.size(), 4U);
  EXPECT_EQ(batches[0].header->mesh, m);
}

TEST(InstanceCoalescer, ThreePartsBelowDefaultThresholdDoNotBatch) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  std::array<DrawPartCmd, 3> parts{make_part(m, mat), make_part(m, mat),
                                   make_part(m, mat)};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 4);
  EXPECT_TRUE(batches.empty());
}

TEST(InstanceCoalescer, ThreePartsBatchWhenMinRunIsTwo) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  std::array<DrawPartCmd, 3> parts{make_part(m, mat), make_part(m, mat),
                                   make_part(m, mat)};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 2);
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches[0].count, 3U);
}

TEST(InstanceCoalescer, DifferentMaterialBreaksRun) {
  auto *m = fake_mesh(1);
  const auto *mat_a = fake_material(2);
  const auto *mat_b = fake_material(3);
  std::array<DrawPartCmd, 6> parts{make_part(m, mat_a), make_part(m, mat_a),
                                   make_part(m, mat_a), make_part(m, mat_b),
                                   make_part(m, mat_b), make_part(m, mat_b)};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 2);
  ASSERT_EQ(batches.size(), 2U);
  EXPECT_EQ(batches[0].header->material, mat_a);
  EXPECT_EQ(batches[1].header->material, mat_b);
  EXPECT_EQ(batches[0].start, 0U);
  EXPECT_EQ(batches[1].start, 3U);
}

TEST(InstanceCoalescer, DifferentTextureBreaksRun) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  auto *tex_a = fake_texture(10);
  auto *tex_b = fake_texture(11);
  std::array<DrawPartCmd, 4> parts{
      make_part(m, mat, tex_a), make_part(m, mat, tex_a),
      make_part(m, mat, tex_b), make_part(m, mat, tex_b)};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 2);
  ASSERT_EQ(batches.size(), 2U);
  EXPECT_EQ(batches[0].header->texture, tex_a);
  EXPECT_EQ(batches[1].header->texture, tex_b);
}

TEST(InstanceCoalescer, MaterialIdBreaksRun) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  std::array<DrawPartCmd, 4> parts{
      make_part(m, mat, nullptr, 0), make_part(m, mat, nullptr, 0),
      make_part(m, mat, nullptr, 3), make_part(m, mat, nullptr, 3)};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 2);
  ASSERT_EQ(batches.size(), 2U);
}

TEST(InstanceCoalescer, TransparentPartsAreNeverFused) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  std::array<DrawPartCmd, 4> parts{
      make_part(m, mat, nullptr, 0, 0.5F), make_part(m, mat, nullptr, 0, 0.5F),
      make_part(m, mat, nullptr, 0, 0.5F), make_part(m, mat, nullptr, 0, 0.5F)};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 2);
  EXPECT_TRUE(batches.empty());
}

TEST(InstanceCoalescer, SkinnedPartsAreNeverFused) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  QMatrix4x4 bone[4]{};
  std::array<DrawPartCmd, 4> parts{make_part(m, mat), make_part(m, mat),
                                   make_part(m, mat), make_part(m, mat)};
  parts[0].palette = Render::GL::BonePaletteRef{bone, 4};
  parts[1].palette = Render::GL::BonePaletteRef{bone, 4};
  const auto batches = Render::Pipeline::coalesce_instances(parts, 2);
  // Parts 0-1 are skinned and disqualified. Parts 2-3 are a run of 2,
  // which matches min_run=2 exactly.
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches[0].start, 2U);
  EXPECT_EQ(batches[0].count, 2U);
}

TEST(InstanceCoalescer, PreservesPerInstanceColorAndAlpha) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  std::array<DrawPartCmd, 4> parts{make_part(m, mat), make_part(m, mat),
                                   make_part(m, mat), make_part(m, mat)};
  parts[0].color = QVector3D(1.0F, 0.0F, 0.0F);
  parts[1].color = QVector3D(0.0F, 1.0F, 0.0F);
  parts[2].color = QVector3D(0.0F, 0.0F, 1.0F);
  parts[3].color = QVector3D(0.5F, 0.5F, 0.5F);
  const auto batches = Render::Pipeline::coalesce_instances(parts, 4);
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches[0].instances[0].color, QVector3D(1.0F, 0.0F, 0.0F));
  EXPECT_EQ(batches[0].instances[3].color, QVector3D(0.5F, 0.5F, 0.5F));
}

TEST(InstanceCoalescer, StatsCountBatchedParts) {
  auto *m = fake_mesh(1);
  const auto *mat = fake_material(2);
  std::array<DrawPartCmd, 10> parts;
  parts.fill(make_part(m, mat));
  const auto batches = Render::Pipeline::coalesce_instances(parts, 4);
  const auto stats = Render::Pipeline::compute_coalesce_stats(batches, 10);
  EXPECT_EQ(stats.input_parts, 10U);
  EXPECT_EQ(stats.batched_parts, 10U);
  EXPECT_EQ(stats.batch_count, 1U);
}
