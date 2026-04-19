// Stage 16.2 — BonePaletteArena UBO pool test.
//
// Verifies the new slab-based UBO arena:
//   * allocate_palette returns distinct CPU pointers per call within a
//     slab and per slot offsets aligned to kPaletteBytes;
//   * offsets reset on reset_frame();
//   * crossing the slab boundary spawns a new slab whose first slot has
//     offset 0 (a separate UBO);
//   * pending_upload_bytes tracks the high-water mark per slab.
//
// Headless (no GL context): ubo == 0, offsets and CPU storage still
// behave correctly.

#include "render/bone_palette_arena.h"

#include <array>

#include <gtest/gtest.h>

namespace {

using Render::GL::BonePaletteArena;

TEST(BonePaletteArenaUBO, OffsetsAdvanceWithinSlab) {
  BonePaletteArena arena;
  auto a = arena.allocate_palette();
  auto b = arena.allocate_palette();
  EXPECT_NE(a.cpu, b.cpu);
  EXPECT_EQ(a.offset, 0);
  EXPECT_EQ(static_cast<std::size_t>(b.offset),
            BonePaletteArena::kPaletteBytes);
  EXPECT_EQ(arena.allocations_in_flight(), 2U);
  // Same slab → same UBO handle (or both 0 in headless).
  EXPECT_EQ(a.ubo, b.ubo);
}

TEST(BonePaletteArenaUBO, ResetReusesStorage) {
  BonePaletteArena arena;
  auto first = arena.allocate_palette();
  arena.reset_frame();
  EXPECT_EQ(arena.allocations_in_flight(), 0U);
  auto reused = arena.allocate_palette();
  EXPECT_EQ(reused.cpu, first.cpu);
  EXPECT_EQ(reused.offset, 0);
}

TEST(BonePaletteArenaUBO, SlabBoundaryStartsFreshUBO) {
  BonePaletteArena arena;
  for (std::size_t i = 0; i < BonePaletteArena::kSlotsPerSlab; ++i) {
    [[maybe_unused]] auto _ = arena.allocate_palette();
  }
  EXPECT_EQ(arena.capacity_slabs(), 1U);

  auto overflow = arena.allocate_palette();
  EXPECT_EQ(arena.capacity_slabs(), 2U);
  // First slot of the second slab restarts offset at 0.
  EXPECT_EQ(overflow.offset, 0);
}

TEST(BonePaletteArenaUBO, PendingBytesTracksHighWater) {
  BonePaletteArena arena;
  EXPECT_EQ(arena.pending_upload_bytes(), 0U);
  [[maybe_unused]] auto a = arena.allocate_palette();
  [[maybe_unused]] auto b = arena.allocate_palette();
  EXPECT_EQ(arena.pending_upload_bytes(), 2U * BonePaletteArena::kPaletteBytes);
  arena.reset_frame();
  EXPECT_EQ(arena.pending_upload_bytes(), 0U);
}

TEST(BonePaletteArenaUBO, PaletteCpuStorageIsIdentityOnAllocation) {
  BonePaletteArena arena;
  auto slot = arena.allocate_palette();
  ASSERT_NE(slot.cpu, nullptr);
  for (std::size_t i = 0; i < BonePaletteArena::kPaletteWidth; ++i) {
    EXPECT_TRUE(slot.cpu[i].isIdentity()) << "slot.cpu[" << i << "]";
  }
}

TEST(BonePaletteArenaUBO, PackedPaletteUsesStd140Mat4Stride) {
  static_assert(BonePaletteArena::kPaletteBytes ==
                BonePaletteArena::kPaletteFloats * sizeof(float));
  static_assert(BonePaletteArena::kPaletteBytes == 4096U);

  std::array<QMatrix4x4, BonePaletteArena::kPaletteWidth> palette{};
  for (auto &matrix : palette) {
    matrix.setToIdentity();
  }

  QMatrix4x4 custom;
  custom.translate(1.0F, 2.0F, 3.0F);
  custom.scale(4.0F, 5.0F, 6.0F);
  palette[3] = custom;

  std::array<float, BonePaletteArena::kPaletteFloats> packed{};
  BonePaletteArena::pack_palette_for_gpu(palette.data(), packed.data());

  const float *expected = custom.constData();
  const std::size_t offset = 3U * BonePaletteArena::kMatrixFloats;
  for (std::size_t i = 0; i < BonePaletteArena::kMatrixFloats; ++i) {
    EXPECT_FLOAT_EQ(packed[offset + i], expected[i]);
  }
}

TEST(BonePaletteArenaUBO, NullPalettePacksIdentityMatrices) {
  std::array<float, BonePaletteArena::kPaletteFloats> packed{};
  BonePaletteArena::pack_palette_for_gpu(nullptr, packed.data());

  QMatrix4x4 identity;
  identity.setToIdentity();
  for (std::size_t bone = 0; bone < BonePaletteArena::kPaletteWidth; ++bone) {
    const std::size_t offset = bone * BonePaletteArena::kMatrixFloats;
    for (std::size_t i = 0; i < BonePaletteArena::kMatrixFloats; ++i) {
      EXPECT_FLOAT_EQ(packed[offset + i], identity.constData()[i]);
    }
  }
}

} // namespace
