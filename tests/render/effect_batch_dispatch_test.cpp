#include <cstddef>
#include <gtest/gtest.h>
#include <string>

#include "render/draw_queue.h"

namespace {

using Render::GL::DrawQueue;
using Render::GL::EffectBatchCmd;
using Render::GL::EffectBatchCmdIndex;
using Render::GL::PreparedBatchKind;

auto submit_effect(DrawQueue& queue, EffectBatchCmd::Kind kind) -> void {
  EffectBatchCmd cmd;
  cmd.kind = kind;
  cmd.intensity = 1.0F;
  cmd.radius = 1.0F;
  queue.submit(cmd);
}

TEST(EffectBatchDispatch, RunsOfOneKindCollapseIntoAnInstancedBatch) {
  for (const auto kind : {EffectBatchCmd::Kind::HealingBeam,
                          EffectBatchCmd::Kind::HealerAura,
                          EffectBatchCmd::Kind::CombatDust,
                          EffectBatchCmd::Kind::BloodPool}) {
    DrawQueue queue;
    submit_effect(queue, kind);
    submit_effect(queue, kind);
    queue.sort_for_batching();

    const auto& batches = queue.prepared_batches();
    ASSERT_EQ(batches.size(), 1U);
    EXPECT_EQ(batches[0].kind, PreparedBatchKind::EffectInstanced)
        << "Two adjacent effects of kind " << static_cast<int>(kind)
        << " form one instanced batch, so the backend's batched path has to "
           "handle every effect kind. A kind missing from that switch renders "
           "nothing at all once two of them are on screen together.";
    EXPECT_EQ(batches[0].count, 2U);
  }
}

TEST(EffectBatchDispatch, ASingleEffectStaysASingleBatchOfOne) {
  DrawQueue queue;
  submit_effect(queue, EffectBatchCmd::Kind::HealingBeam);
  queue.sort_for_batching();

  const auto& batches = queue.prepared_batches();
  ASSERT_EQ(batches.size(), 1U);
  EXPECT_EQ(batches[0].kind, PreparedBatchKind::Single);
  EXPECT_EQ(batches[0].count, 1U)
      << "A non-instanced effect batch always covers exactly one command, which "
         "is what lets the backend treat the single case as a batch of one.";
}

TEST(EffectBatchDispatch, DifferentKindsDoNotShareABatch) {
  DrawQueue queue;
  submit_effect(queue, EffectBatchCmd::Kind::HealerAura);
  submit_effect(queue, EffectBatchCmd::Kind::HealingBeam);
  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 2U);
  for (const auto& batch : queue.prepared_batches()) {
    EXPECT_EQ(batch.count, 1U)
        << "Effects only batch with their own kind; the sort key packs the kind.";
  }
}

} // namespace
