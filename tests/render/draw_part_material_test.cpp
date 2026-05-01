

#include "render/draw_part.h"
#include "render/draw_queue.h"
#include "render/material.h"
#include "render/submitter.h"

#include <gtest/gtest.h>

namespace {

auto fake_shader(std::uintptr_t tag) -> Render::GL::Shader * {
  return reinterpret_cast<Render::GL::Shader *>(tag);
}
auto fake_mesh(std::uintptr_t tag) -> Render::GL::Mesh * {
  return reinterpret_cast<Render::GL::Mesh *>(tag);
}
auto fake_texture(std::uintptr_t tag) -> Render::GL::Texture * {
  return reinterpret_cast<Render::GL::Texture *>(tag);
}

} // namespace

TEST(DrawPartCmdSortOrder, SitsBetweenTerrainAndSelectionRing) {
  Render::GL::DrawQueue queue;

  Render::GL::TerrainSurfaceCmd terrain{};
  terrain.mesh = fake_mesh(0x1000);

  Render::GL::Material mat{};
  Render::GL::DrawPartCmd part{};
  part.mesh = fake_mesh(0x2000);
  part.material = &mat;

  Render::GL::SelectionRingCmd ring{};

  queue.submit(ring);
  queue.submit(part);
  queue.submit(terrain);
  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 3U);
  EXPECT_EQ(queue.get_sorted(0).index(), Render::GL::TerrainSurfaceCmdIndex);
  EXPECT_EQ(queue.get_sorted(1).index(), Render::GL::DrawPartCmdIndex);
  EXPECT_EQ(queue.get_sorted(2).index(), Render::GL::SelectionRingCmdIndex);
}

TEST(DrawPartCmdSortOrder, BatchesByMaterialThenMesh) {
  Render::GL::DrawQueue queue;

  Render::GL::Material mat_a{};
  Render::GL::Material mat_b{};

  Render::GL::DrawPartCmd a{};
  a.material = &mat_a;
  a.mesh = fake_mesh(0x1110);

  Render::GL::DrawPartCmd b{};
  b.material = &mat_a;
  b.mesh = fake_mesh(0x1110);

  Render::GL::DrawPartCmd c{};
  c.material = &mat_b;
  c.mesh = fake_mesh(0x1110);

  queue.submit(a);
  queue.submit(c);
  queue.submit(b);
  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 3U);
  for (std::size_t idx = 0; idx < 3U; ++idx) {
    EXPECT_EQ(queue.get_sorted(idx).index(), Render::GL::DrawPartCmdIndex);
  }
  auto is_mat_a = [&](std::size_t i) {
    return std::get<Render::GL::DrawPartCmdIndex>(queue.get_sorted(i))
               .material == &mat_a;
  };

  bool const adjacent_a =
      (is_mat_a(0) && is_mat_a(1)) || (is_mat_a(1) && is_mat_a(2));
  EXPECT_TRUE(adjacent_a);
}

TEST(DrawPartCmdSortOrder, FullKeyGroupsSameMeshWithinMaterial) {
  Render::GL::DrawQueue queue;

  Render::GL::Material mat{};
  Render::GL::DrawPartCmd a{};
  a.material = &mat;
  a.mesh = fake_mesh(0x1110);

  Render::GL::DrawPartCmd b{};
  b.material = &mat;
  b.mesh = fake_mesh(0x2220);

  Render::GL::DrawPartCmd c{};
  c.material = &mat;
  c.mesh = fake_mesh(0x1110);

  queue.submit(a);
  queue.submit(b);
  queue.submit(c);
  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 3U);
  const auto &first =
      std::get<Render::GL::DrawPartCmdIndex>(queue.get_sorted(0));
  const auto &second =
      std::get<Render::GL::DrawPartCmdIndex>(queue.get_sorted(1));
  EXPECT_EQ(first.mesh, fake_mesh(0x1110));
  EXPECT_EQ(second.mesh, fake_mesh(0x1110));
  EXPECT_LE(queue.sort_key_for_sorted(0), queue.sort_key_for_sorted(1));
  EXPECT_LE(queue.sort_key_for_sorted(1), queue.sort_key_for_sorted(2));
}

TEST(DrawQueuePreparedBatches, MeshCommandsExposeInstancedBatch) {
  Render::GL::DrawQueue queue;

  Render::GL::MeshCmd a{};
  a.shader = fake_shader(0xA00);
  a.mesh = fake_mesh(0x1000);
  a.texture = fake_texture(0xD00);

  Render::GL::MeshCmd b = a;
  Render::GL::MeshCmd other = a;
  other.mesh = fake_mesh(0x2000);

  queue.submit(a);
  queue.submit(other);
  queue.submit(b);
  queue.sort_for_batching();

  const auto &batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 2U);
  EXPECT_EQ(batches[0].kind, Render::GL::PreparedBatchKind::MeshInstanced);
  EXPECT_EQ(batches[0].type, Render::GL::DrawCmdType::Mesh);
  EXPECT_EQ(batches[0].count, 2U);
  EXPECT_EQ(batches[1].kind, Render::GL::PreparedBatchKind::Single);
  EXPECT_EQ(batches[1].count, 1U);
}

TEST(DrawQueuePreparedBatches, DrawPartMinRunBecomesPreparedBatch) {
  Render::GL::DrawQueue queue;
  Render::GL::Material mat{};

  Render::GL::DrawPartCmd common{};
  common.material = &mat;
  common.mesh = fake_mesh(0x1110);
  common.texture = fake_texture(0x9000);
  common.material_id = 7;

  Render::GL::DrawPartCmd other = common;
  other.mesh = fake_mesh(0x2220);

  queue.submit(common);
  queue.submit(other);
  queue.submit(common);
  queue.submit(common);
  queue.submit(common);
  queue.sort_for_batching();

  const auto &batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 2U);
  EXPECT_EQ(batches[0].kind, Render::GL::PreparedBatchKind::DrawPartInstanced);
  EXPECT_EQ(batches[0].count, 4U);
  EXPECT_EQ(batches[1].kind, Render::GL::PreparedBatchKind::Single);
}

TEST(MaterialResolve, PicksExactTierWhenAvailable) {
  Render::GL::Material mat{};
  mat.shader_full = fake_shader(1);
  mat.shader_reduced = fake_shader(2);
  mat.shader_minimal = fake_shader(3);

  EXPECT_EQ(mat.resolve(Render::ShaderQuality::Full), fake_shader(1));
  EXPECT_EQ(mat.resolve(Render::ShaderQuality::Reduced), fake_shader(2));
  EXPECT_EQ(mat.resolve(Render::ShaderQuality::Minimal), fake_shader(3));
}

TEST(MaterialResolve, NoneAlwaysReturnsNullptr) {
  Render::GL::Material mat{};
  mat.shader_full = fake_shader(1);
  mat.shader_reduced = fake_shader(2);
  mat.shader_minimal = fake_shader(3);

  EXPECT_EQ(mat.resolve(Render::ShaderQuality::None), nullptr);
}

TEST(MaterialResolve, FallsBackWhenRequestedTierMissing) {

  Render::GL::Material full_only{};
  full_only.shader_full = fake_shader(10);

  EXPECT_EQ(full_only.resolve(Render::ShaderQuality::Full), fake_shader(10));
  EXPECT_EQ(full_only.resolve(Render::ShaderQuality::Reduced), fake_shader(10));
  EXPECT_EQ(full_only.resolve(Render::ShaderQuality::Minimal), fake_shader(10));

  Render::GL::Material minimal_only{};
  minimal_only.shader_minimal = fake_shader(20);

  EXPECT_EQ(minimal_only.resolve(Render::ShaderQuality::Full), fake_shader(20));
  EXPECT_EQ(minimal_only.resolve(Render::ShaderQuality::Reduced),
            fake_shader(20));
  EXPECT_EQ(minimal_only.resolve(Render::ShaderQuality::Minimal),
            fake_shader(20));
}

TEST(MaterialResolve, AllTiersEmptyReturnsNullptr) {
  Render::GL::Material mat{};
  EXPECT_EQ(mat.resolve(Render::ShaderQuality::Full), nullptr);
  EXPECT_EQ(mat.resolve(Render::ShaderQuality::Reduced), nullptr);
  EXPECT_EQ(mat.resolve(Render::ShaderQuality::Minimal), nullptr);
  EXPECT_EQ(mat.resolve(Render::ShaderQuality::None), nullptr);
  EXPECT_TRUE(mat.is_flat_only());
}

TEST(QueueSubmitterPart, EmitsDrawPartCmdWithPerInstanceOverrides) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);

  Render::GL::Material mat{};
  mat.shader_full = fake_shader(0xAA);

  QMatrix4x4 model;
  model.translate(1.0F, 2.0F, 3.0F);
  QVector3D const tint(0.25F, 0.5F, 0.75F);

  submitter.part(fake_mesh(0xBEEF), &mat, model, tint, nullptr, 0.4F, 42);

  ASSERT_EQ(queue.size(), 1U);
  auto const &cmd = queue.items().front();
  ASSERT_EQ(cmd.index(), Render::GL::DrawPartCmdIndex);
  auto const &part = std::get<Render::GL::DrawPartCmdIndex>(cmd);
  EXPECT_EQ(part.material, &mat);
  EXPECT_EQ(part.mesh, fake_mesh(0xBEEF));
  EXPECT_FLOAT_EQ(part.color.x(), 0.25F);
  EXPECT_FLOAT_EQ(part.alpha, 0.4F);
  EXPECT_EQ(part.material_id, 42);
}

TEST(QueueSubmitterPart, NullMaterialFallsBackToMeshCmd) {
  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter submitter(&queue);

  QMatrix4x4 model;
  submitter.part(fake_mesh(0x1234), nullptr, model,
                 QVector3D(1.0F, 1.0F, 1.0F));

  ASSERT_EQ(queue.size(), 1U);
  EXPECT_EQ(queue.items().front().index(), Render::GL::MeshCmdIndex);
}
