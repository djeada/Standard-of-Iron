// Stage 15.5b — rigged pipeline / RiggedCreatureCmd queue-dispatch tests.
//
// No headless GL context is available in this test harness, so we can't
// drive RiggedCharacterPipeline::draw() against a real shader program.
// What we *can* verify is that:
//
//   1. The new RiggedCreatureCmd variant slot is wired into the DrawCmd
//      std::variant, carries the expected defaults, and its DrawCmdType
//      enum index matches the variant index.
//   2. DrawQueue::submit + sort_for_batching accept the variant without
//      crashing and preserve a RiggedCreatureCmd through the sort path.
//   3. The k_type_order table has been extended to 14 entries, so
//      compute_sort_key does not index out of bounds for a
//      RiggedCreatureCmd.
//
// When a headless-GL fixture lands (Stage 16 / test infrastructure work),
// a second test pass can exercise initialize()/draw() against a live
// context. For now this test guarantees the variant compiles, dispatches
// through the queue, and does not silently fall off the type-order table.

#include "render/draw_queue.h"

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
  EXPECT_EQ(std::variant_size_v<Render::GL::DrawCmd>, 14U);
}

TEST(RiggedPipeline, DefaultsAreSane) {
  Render::GL::RiggedCreatureCmd cmd;
  EXPECT_EQ(cmd.mesh, nullptr);
  EXPECT_EQ(cmd.material, nullptr);
  EXPECT_EQ(cmd.bone_palette, nullptr);
  EXPECT_EQ(cmd.bone_count, 0U);
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

  // sort_for_batching must accept the new variant without tripping on
  // the k_type_order table (which now has 14 entries).
  queue.sort_for_batching();
  const DrawCmd &sorted = queue.get_sorted(0);
  EXPECT_EQ(sorted.index(), RiggedCreatureCmdIndex);
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
  // extract_cmd_priority must compile for the new variant.
  for (std::size_t i = 0; i < queue.size(); ++i) {
    EXPECT_NO_THROW((void)extract_cmd_priority(queue.get_sorted(i)));
  }
}

} // namespace
