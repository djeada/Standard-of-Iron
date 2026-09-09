#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QVector4D>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/map/visibility_service.h"
#include "render/gl/buffer.h"
#include "render/ground/scatter_renderer_state.h"
#include "render/profiling/asset_counters.h"

namespace {

using Render::Ground::Scatter::ScatterMemoryMode;
using Render::Profiling::asset_counters;
using Render::Profiling::AssetCounter;

struct OffscreenGl {
  QOffscreenSurface surface;
  QOpenGLContext context;
  bool ready = false;

  OffscreenGl() {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    surface.setFormat(format);
    surface.create();
    if (!surface.isValid()) {
      return;
    }
    context.setFormat(format);
    if (!context.create() || !context.makeCurrent(&surface)) {
      return;
    }
    ready = true;
  }
  OffscreenGl(const OffscreenGl&) = delete;
  OffscreenGl(OffscreenGl&&) = delete;
  auto operator=(const OffscreenGl&) -> OffscreenGl& = delete;
  auto operator=(OffscreenGl&&) -> OffscreenGl& = delete;
  ~OffscreenGl() {
    if (ready) {
      context.doneCurrent();
    }
  }
};

struct TestInstance {
  QVector4D pos_scale;
};

struct TestParams {
  float time = 0.0F;
};

using State = Render::Ground::Scatter::FilteredRendererState<TestInstance, TestParams>;

constexpr int k_grid_cells = 256;
constexpr float k_tile_size = 1.0F;
constexpr float k_chunk = Render::Ground::Scatter::k_chunk_world_size;
constexpr int k_first_chunk = -2;
constexpr int k_last_chunk = 2;
constexpr int k_instances_per_chunk = 8;

auto position_of(const TestInstance& instance) -> const QVector4D& {
  return instance.pos_scale;
}

auto make_snapshot() -> Game::Map::VisibilityService::Snapshot {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.version = 1;
  snapshot.initialized = true;
  snapshot.width = k_grid_cells;
  snapshot.height = k_grid_cells;
  snapshot.tile_size = k_tile_size;
  snapshot.half_width = static_cast<float>(k_grid_cells) * 0.5F;
  snapshot.half_height = static_cast<float>(k_grid_cells) * 0.5F;
  snapshot.cells.assign(static_cast<std::size_t>(k_grid_cells) *
                            static_cast<std::size_t>(k_grid_cells),
                        static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
  return snapshot;
}

void reveal_world_band(Game::Map::VisibilityService::Snapshot& snapshot,
                       float world_min_x,
                       float world_max_x) {
  ++snapshot.version;
  const auto to_grid = [&](float world_x) {
    return static_cast<int>(world_x / snapshot.tile_size + snapshot.half_width);
  };
  const int first = std::max(0, to_grid(world_min_x));
  const int last = std::min(snapshot.width - 1, to_grid(world_max_x));
  std::fill(snapshot.cells.begin(),
            snapshot.cells.end(),
            static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
  for (int z = 0; z < snapshot.height; ++z) {
    for (int x = first; x <= last; ++x) {
      snapshot.cells[static_cast<std::size_t>(z) *
                         static_cast<std::size_t>(snapshot.width) +
                     static_cast<std::size_t>(x)] =
          static_cast<std::uint8_t>(Game::Map::VisibilityState::Visible);
    }
  }
}

void hide_everything(Game::Map::VisibilityService::Snapshot& snapshot) {
  ++snapshot.version;
  std::fill(snapshot.cells.begin(),
            snapshot.cells.end(),
            static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
}

auto make_state() -> State {
  State state;
  for (int chunk_index = k_first_chunk; chunk_index <= k_last_chunk; ++chunk_index) {
    const float base_x = static_cast<float>(chunk_index) * k_chunk + 2.0F;
    for (int step = 0; step < k_instances_per_chunk; ++step) {
      state.instances.push_back(
          TestInstance{QVector4D(base_x + static_cast<float>(step),
                                 0.0F,
                                 static_cast<float>(step) * 2.0F - 8.0F,
                                 1.0F)});
    }
  }
  state.instance_count = state.instances.size();
  state.instances_dirty = true;
  return state;
}

auto retained_buffer_bytes(const State& state) -> std::size_t {
  std::size_t bytes = 0;
  for (const auto& chunk : state.spatial_chunks) {
    if (chunk.buffer != nullptr) {
      bytes += chunk.buffer->size_bytes();
    }
  }
  return bytes;
}

auto chunks_with_buffers(const State& state) -> std::size_t {
  return static_cast<std::size_t>(std::count_if(
      state.spatial_chunks.begin(), state.spatial_chunks.end(), [](const auto& chunk) {
        return chunk.buffer != nullptr;
      }));
}

auto submitted_instances(const State& state) -> std::size_t {
  std::size_t submitted = 0;
  for (const auto& chunk : state.spatial_chunks) {
    if (chunk.visible_count == 0 || chunk.buffer == nullptr) {
      continue;
    }
    submitted += chunk.visible_count;
  }
  return submitted;
}

void prewarm_chunk_buffers(State& state) {
  for (auto& chunk : state.spatial_chunks) {
    if (chunk.buffer == nullptr) {
      chunk.buffer =
          std::make_unique<Render::GL::Buffer>(Render::GL::Buffer::Type::Vertex);
    }
    chunk.buffer->bind();
    chunk.buffer->unbind();
  }
}

auto sync(State& state,
          const Game::Map::VisibilityService::Snapshot& snapshot,
          ScatterMemoryMode mode) -> std::uint32_t {
  return Render::Ground::Scatter::sync_filtered_state(
      state, position_of, &snapshot, mode);
}

class ScatterGpuRetention : public ::testing::Test {
protected:
  void SetUp() override {
    if (!m_gl.ready) {
      GTEST_SKIP() << "no OpenGL 3.3 core context is available";
    }
    asset_counters().reset();
  }

  static auto buffers_created() -> std::uint64_t {
    return asset_counters().total(AssetCounter::GlBufferCreated);
  }
  static auto transfer_bytes() -> std::uint64_t {
    return asset_counters().total(AssetCounter::GlBufferTransferBytes);
  }
  static auto storage_bytes() -> std::uint64_t {
    return asset_counters().total(AssetCounter::GlBufferStorageBytes);
  }

  OffscreenGl m_gl;
};

TEST_F(ScatterGpuRetention, RevealHideRevealRetainsBuffersAndHidesInstances) {
  State state = make_state();
  auto snapshot = make_snapshot();

  reveal_world_band(snapshot, -80.0F, 80.0F);
  const std::uint32_t revealed = sync(state, snapshot, ScatterMemoryMode::VisibleOnly);
  ASSERT_EQ(revealed, state.instances.size());
  const std::uint64_t created_after_reveal = buffers_created();
  const std::uint64_t transferred_after_reveal = transfer_bytes();
  const std::size_t resident_chunks = chunks_with_buffers(state);
  const std::size_t resident_bytes = retained_buffer_bytes(state);
  EXPECT_EQ(resident_chunks, state.spatial_chunks.size());
  EXPECT_GT(created_after_reveal, 0U);
  EXPECT_EQ(transferred_after_reveal, state.instances.size() * sizeof(TestInstance));
  EXPECT_EQ(resident_bytes, state.instances.size() * sizeof(TestInstance));
  EXPECT_EQ(submitted_instances(state), revealed);

  hide_everything(snapshot);
  const std::uint32_t hidden = sync(state, snapshot, ScatterMemoryMode::VisibleOnly);
  EXPECT_EQ(hidden, 0U);
  EXPECT_EQ(submitted_instances(state), 0U)
      << "hidden chunks must not submit their retained instances";
  EXPECT_EQ(chunks_with_buffers(state), resident_chunks)
      << "hiding must retain the allocated chunk buffers";
  EXPECT_EQ(retained_buffer_bytes(state), resident_bytes)
      << "hiding must not shrink or free retained chunk memory";
  EXPECT_EQ(buffers_created(), created_after_reveal) << "hiding must not allocate";
  EXPECT_EQ(transfer_bytes(), transferred_after_reveal) << "hiding must not upload";

  reveal_world_band(snapshot, -80.0F, 80.0F);
  const std::uint32_t revealed_again =
      sync(state, snapshot, ScatterMemoryMode::VisibleOnly);
  EXPECT_EQ(revealed_again, revealed);
  EXPECT_EQ(submitted_instances(state), revealed);
  EXPECT_EQ(buffers_created(), created_after_reveal)
      << "re-revealing known chunks must reuse the retained buffers";
  EXPECT_EQ(retained_buffer_bytes(state), resident_bytes);
}

TEST_F(ScatterGpuRetention, PrewarmedChunksSubmitNothingUntilTheyAreRevealed) {
  State state = make_state();
  auto snapshot = make_snapshot();

  reveal_world_band(snapshot, -80.0F, 80.0F);
  sync(state, snapshot, ScatterMemoryMode::VisibleOnly);
  hide_everything(snapshot);
  sync(state, snapshot, ScatterMemoryMode::VisibleOnly);
  ASSERT_EQ(submitted_instances(state), 0U);

  asset_counters().reset();
  prewarm_chunk_buffers(state);
  EXPECT_EQ(buffers_created(), 0U)
      << "prewarming already-resident chunks must not allocate again";
  EXPECT_EQ(storage_bytes(), 0U) << "prewarming must not reallocate chunk storage";
  EXPECT_EQ(submitted_instances(state), 0U)
      << "a prewarmed hidden chunk must stay unsubmitted";

  const float narrow_min = static_cast<float>(k_first_chunk) * k_chunk;
  reveal_world_band(snapshot, narrow_min, narrow_min + 4.0F);
  const std::uint32_t narrow = sync(state, snapshot, ScatterMemoryMode::VisibleOnly);
  EXPECT_GT(narrow, 0U);
  EXPECT_LT(narrow, state.instances.size());
  EXPECT_EQ(submitted_instances(state), narrow)
      << "only the revealed chunk may submit instances";
}

TEST_F(ScatterGpuRetention, RepeatedChunkTraversalStopsAllocating) {
  State state = make_state();
  auto snapshot = make_snapshot();

  const auto traverse = [&](ScatterMemoryMode mode) {
    std::uint32_t seen = 0;
    for (int step = k_first_chunk; step <= k_last_chunk; ++step) {
      const float centre = static_cast<float>(step) * k_chunk;
      reveal_world_band(snapshot, centre - k_chunk, centre + k_chunk);
      seen = std::max(seen, sync(state, snapshot, mode));
    }
    return seen;
  };

  const std::uint32_t first_pass = traverse(ScatterMemoryMode::VisibleOnly);
  ASSERT_GT(first_pass, 0U);
  const std::uint64_t created_after_first = buffers_created();
  const std::size_t bytes_after_first = retained_buffer_bytes(state);
  EXPECT_EQ(chunks_with_buffers(state), state.spatial_chunks.size())
      << "a full traversal should have made every chunk resident";

  const std::uint64_t transferred_before_second = transfer_bytes();
  const std::uint32_t second_pass = traverse(ScatterMemoryMode::VisibleOnly);
  const std::uint64_t second_pass_bytes = transfer_bytes() - transferred_before_second;
  EXPECT_EQ(second_pass, first_pass);
  EXPECT_EQ(buffers_created(), created_after_first)
      << "a repeated traversal must not allocate new chunk buffers";
  EXPECT_EQ(retained_buffer_bytes(state), bytes_after_first)
      << "retained chunk memory must be stable across traversals";

  const std::uint64_t transferred_before_third = transfer_bytes();
  const std::uint32_t third_pass = traverse(ScatterMemoryMode::VisibleOnly);
  const std::uint64_t third_pass_bytes = transfer_bytes() - transferred_before_third;
  EXPECT_EQ(third_pass, first_pass);
  EXPECT_EQ(buffers_created(), created_after_first);
  EXPECT_EQ(third_pass_bytes, second_pass_bytes)
      << "per-traversal upload volume must not grow";
}

TEST_F(ScatterGpuRetention, RememberedTraversalConvergesToNoGpuWork) {
  State state = make_state();
  auto snapshot = make_snapshot();

  const auto traverse = [&]() {
    for (int step = k_first_chunk; step <= k_last_chunk; ++step) {
      const float centre = static_cast<float>(step) * k_chunk;
      reveal_world_band(snapshot, centre - k_chunk, centre + k_chunk);
      sync(state, snapshot, ScatterMemoryMode::Remembered);
    }
  };

  traverse();
  const std::uint64_t created_after_first = buffers_created();
  const std::size_t bytes_after_first = retained_buffer_bytes(state);
  ASSERT_GT(created_after_first, 0U);

  const std::uint64_t transferred_before_second = transfer_bytes();
  traverse();
  EXPECT_EQ(buffers_created(), created_after_first)
      << "remembered chunks must not reallocate on a second traversal";
  EXPECT_EQ(transfer_bytes(), transferred_before_second)
      << "fully remembered chunks must not re-upload";
  EXPECT_EQ(retained_buffer_bytes(state), bytes_after_first);
  EXPECT_EQ(submitted_instances(state), state.instances.size())
      << "remembered scatter stays submitted once explored";
}

} // namespace
