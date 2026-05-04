

#include "render/bone_palette_arena.h"
#include "render/draw_queue.h"
#include "render/gl/backend/rigged_character_pipeline.h"
#include "render/material.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::GL::RiggedCreatureCmd;
using Render::GL::BackendPipelines::RiggedCharacterPipeline;

auto k_mesh_a = reinterpret_cast<Render::GL::RiggedMesh *>(
    static_cast<std::uintptr_t>(0x1000));
auto k_mesh_b = reinterpret_cast<Render::GL::RiggedMesh *>(
    static_cast<std::uintptr_t>(0x2000));
auto k_mat_a = reinterpret_cast<const Render::GL::Material *>(
    static_cast<std::uintptr_t>(0x3000));
auto k_mat_b = reinterpret_cast<const Render::GL::Material *>(
    static_cast<std::uintptr_t>(0x4000));
auto k_tex = reinterpret_cast<Render::GL::Texture *>(
    static_cast<std::uintptr_t>(0x5000));

auto make_cmd(Render::GL::RiggedMesh *mesh, const Render::GL::Material *mat,
              Render::GL::Texture *tex = nullptr,
              std::uint32_t palette_ubo = 1U,
              std::uint32_t palette_slot = 0U) -> RiggedCreatureCmd {
  RiggedCreatureCmd c;
  c.mesh = mesh;
  c.material = mat;
  c.texture = tex;
  c.palette_ubo = palette_ubo;
  c.palette_offset =
      palette_slot *
      static_cast<std::uint32_t>(Render::GL::BonePaletteArena::kPaletteBytes);
  c.world.setToIdentity();
  return c;
}

TEST(RiggedPipelineInstanced, TenCompatibleCmdsCoalesceIntoOneGroup) {
  std::vector<RiggedCreatureCmd> cmds;
  for (int i = 0; i < 10; ++i) {
    cmds.push_back(make_cmd(k_mesh_a, k_mat_a));
  }
  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), 64, groups);
  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0], 10U);
}

TEST(RiggedPipelineInstanced, MixedMaterialsSplitIntoMultipleGroups) {
  std::vector<RiggedCreatureCmd> cmds;
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_b));
  cmds.push_back(make_cmd(k_mesh_b, k_mat_b));
  cmds.push_back(make_cmd(k_mesh_b, k_mat_b));

  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), 64, groups);
  ASSERT_GE(groups.size(), 2U);
  std::size_t sum = 0;
  for (auto g : groups) {
    sum += g;
  }
  EXPECT_EQ(sum, cmds.size());

  EXPECT_EQ(groups.front(), 2U);
}

TEST(RiggedPipelineInstanced, TexturedCmdsDoNotInstance) {
  std::vector<RiggedCreatureCmd> cmds;
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, k_tex));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, k_tex));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a));

  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), 64, groups);

  ASSERT_EQ(groups.size(), 3U);
  EXPECT_EQ(groups[0], 1U);
  EXPECT_EQ(groups[1], 1U);
  EXPECT_EQ(groups[2], 1U);
}

TEST(RiggedPipelineInstanced, SingleCmdEmitsSizeOneGroup) {
  std::vector<RiggedCreatureCmd> cmds;
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a));
  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), 64, groups);
  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0], 1U);
}

TEST(RiggedPipelineInstanced, CapBoundsLargeCompatibleRun) {
  std::vector<RiggedCreatureCmd> cmds;
  for (int i = 0; i < 200; ++i) {
    cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 1U,
                            static_cast<std::uint32_t>(i)));
  }
  constexpr std::size_t k_cap = 16;
  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), k_cap,
                                          groups);

  ASSERT_EQ(groups.size(), 13U);
  for (std::size_t k = 0; k < 12; ++k) {
    EXPECT_EQ(groups[k], k_cap) << "group " << k;
  }
  EXPECT_EQ(groups.back(), 8U);
}

TEST(RiggedPipelineInstanced, RolePaletteMismatchSplitsGroups) {
  std::vector<RiggedCreatureCmd> cmds;
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 7U, 0U));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 7U, 1U));
  cmds[0].role_color_count = 1;
  cmds[1].role_color_count = 1;
  cmds[0].role_colors[0] = QVector3D(1.0F, 0.0F, 0.0F);
  cmds[1].role_colors[0] = QVector3D(0.0F, 0.0F, 1.0F);

  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), 8U, groups);

  ASSERT_EQ(groups.size(), 2U);
  EXPECT_EQ(groups[0], 1U);
  EXPECT_EQ(groups[1], 1U);
}

TEST(RiggedPipelineInstanced, PaletteRangeOverflowSplitsGroups) {
  std::vector<RiggedCreatureCmd> cmds;
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 3U, 0U));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 3U, 1U));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 3U, 5U));

  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), 4U, groups);

  ASSERT_EQ(groups.size(), 2U);
  EXPECT_EQ(groups[0], 2U);
  EXPECT_EQ(groups[1], 1U);
}

TEST(RiggedPipelineInstanced, MissingPaletteUboDisablesInstancing) {
  std::vector<RiggedCreatureCmd> cmds;
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 0U, 0U));
  cmds.push_back(make_cmd(k_mesh_a, k_mat_a, nullptr, 0U, 1U));

  std::vector<std::size_t> groups;
  RiggedCharacterPipeline::compute_groups(cmds.data(), cmds.size(), 8U, groups);

  ASSERT_EQ(groups.size(), 2U);
  EXPECT_EQ(groups[0], 1U);
  EXPECT_EQ(groups[1], 1U);
}

TEST(RiggedPipelineInstanced, PaletteRangeCoversDeclaredShaderBatch) {
  constexpr std::size_t k_shader_batch_size = 16;
  EXPECT_EQ(RiggedCharacterPipeline::palette_range_bytes_for_instanced_shader(
                k_shader_batch_size),
            k_shader_batch_size * Render::GL::BonePaletteArena::kPaletteBytes);
}

TEST(RiggedPipelineInstanced, HeadlessDrawInstancedRecordsBatchSize) {
  RiggedCharacterPipeline pipe(nullptr, nullptr);

  std::vector<RiggedCreatureCmd> cmds(7, make_cmd(k_mesh_a, k_mat_a));

  bool ok = pipe.draw_instanced(cmds.data(), cmds.size(), QMatrix4x4{});
  EXPECT_FALSE(ok);
  EXPECT_TRUE(pipe.batch_sizes_for_test().empty());
}

} // namespace
