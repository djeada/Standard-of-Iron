

#include "render/draw_queue.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <variant>

namespace {

TEST(RiggedPipeline, VariantIndexMatchesEnum) {
  Render::GL::RiggedCreatureCmd cmd;
  Render::GL::DrawCmd v = cmd;
  EXPECT_EQ(v.index(), Render::GL::RiggedCreatureCmdIndex);
  EXPECT_EQ(static_cast<std::size_t>(Render::GL::DrawCmdType::RiggedCreature),
            Render::GL::RiggedCreatureCmdIndex);
  EXPECT_EQ(std::variant_size_v<Render::GL::DrawCmd>, 15U);
}

TEST(RiggedPipeline, DefaultsAreSane) {
  Render::GL::RiggedCreatureCmd cmd;
  EXPECT_EQ(cmd.mesh, nullptr);
  EXPECT_EQ(cmd.material, nullptr);
  EXPECT_EQ(cmd.bone_palette, nullptr);
  EXPECT_EQ(cmd.bone_count, 0U);
  EXPECT_EQ(cmd.role_color_count, 0U);
  EXPECT_EQ(cmd.texture, nullptr);
  EXPECT_EQ(cmd.material_id, 0);
  EXPECT_FLOAT_EQ(cmd.alpha, 1.0F);
  EXPECT_EQ(cmd.color, QVector3D(1.0F, 1.0F, 1.0F));
  EXPECT_EQ(cmd.variation_scale, QVector3D(1.0F, 1.0F, 1.0F));
  EXPECT_EQ(cmd.priority, Render::CommandPriority::Normal);
}

TEST(RiggedPipeline, DrawQueueSubmitAndSort) {
  using namespace Render::GL;

  std::array<QMatrix4x4, 4> palette{};
  for (auto &m : palette) {
    m.setToIdentity();
  }

  RiggedCreatureCmd rigged;
  rigged.world.setToIdentity();
  rigged.world.translate(1.0F, 2.0F, 3.0F);
  rigged.bone_palette = palette.data();
  rigged.bone_count = static_cast<std::uint32_t>(palette.size());
  rigged.color = QVector3D(0.5F, 0.25F, 0.125F);
  rigged.alpha = 0.75F;
  rigged.variation_scale = QVector3D(1.1F, 0.9F, 1.0F);
  rigged.material_id = 42;

  DrawQueue queue;
  queue.submit(rigged);
  ASSERT_EQ(queue.size(), 1U);

  const DrawCmd &stored = queue.items().front();
  ASSERT_EQ(stored.index(), RiggedCreatureCmdIndex);
  const auto &round_trip = std::get<RiggedCreatureCmdIndex>(stored);
  EXPECT_EQ(round_trip.bone_count, 4U);
  EXPECT_EQ(round_trip.bone_palette, palette.data());
  EXPECT_FLOAT_EQ(round_trip.alpha, 0.75F);
  EXPECT_EQ(round_trip.material_id, 42);

  queue.sort_for_batching();
  const DrawCmd &sorted = queue.get_sorted(0);
  EXPECT_EQ(sorted.index(), RiggedCreatureCmdIndex);
}

TEST(RiggedPipeline, SelectionRingSortsAfterRiggedCreatures) {
  using namespace Render::GL;

  DrawQueue queue;

  SelectionRingCmd ring;
  ring.priority = Render::CommandPriority::Critical;
  queue.submit(ring);

  RiggedCreatureCmd rigged;
  rigged.priority = Render::CommandPriority::Low;
  queue.submit(rigged);

  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 2U);
  EXPECT_EQ(queue.get_sorted(0).index(), RiggedCreatureCmdIndex);
  EXPECT_EQ(queue.get_sorted(1).index(), SelectionRingCmdIndex);
}

TEST(RiggedPipeline, PrioritySortKeyPathMultipleCmds) {
  using namespace Render::GL;

  DrawQueue queue;

  GridCmd grid;
  queue.submit(grid);

  RiggedCreatureCmd rigged_a;
  rigged_a.priority = Render::CommandPriority::Normal;
  queue.submit(rigged_a);

  RiggedCreatureCmd rigged_b;
  rigged_b.priority = Render::CommandPriority::Normal;
  queue.submit(rigged_b);

  queue.sort_for_batching();
  ASSERT_EQ(queue.size(), 3U);

  for (std::size_t i = 0; i < queue.size(); ++i) {
    EXPECT_NO_THROW((void)extract_cmd_priority(queue.get_sorted(i)));
  }
}

TEST(RiggedPipeline, QueueSubmitterShaderStateDoesNotAffectRiggedBatching) {
  using namespace Render::GL;

  DrawQueue queue;
  QueueSubmitter submitter(&queue);

  auto *mesh = reinterpret_cast<RiggedMesh *>(0x1000);
  auto *shader_a = reinterpret_cast<Shader *>(0x2000);
  auto *shader_b = reinterpret_cast<Shader *>(0x3000);

  RiggedCreatureCmd first;
  first.mesh = mesh;
  first.bone_count = 12;

  RiggedCreatureCmd second = first;

  submitter.set_shader(shader_a);
  submitter.rigged(first);
  submitter.set_shader(shader_b);
  submitter.rigged(second);

  ASSERT_EQ(queue.size(), 2U);

  queue.sort_for_batching();
  const auto &batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches.front().kind, PreparedBatchKind::RiggedCreatureInstanced);
  EXPECT_EQ(batches.front().count, 2U);
}

} // namespace
