

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <vector>

#include "render/draw_queue.h"
#include "render/frame_budget.h"

namespace {

using Render::GL::DrawQueue;
using Render::GL::GroundMarkerCmd;
using Render::GL::Mesh;
using Render::GL::MeshCmd;
using Render::GL::Shader;
using Render::GL::TerrainSurfaceCmd;
using Render::GL::Texture;

auto fake_mesh(std::uintptr_t index) -> Mesh* {
  return reinterpret_cast<Mesh*>(0x1000U + index * 0x40U);
}
auto fake_shader(std::uintptr_t index) -> Shader* {
  return reinterpret_cast<Shader*>(0x9000U + index * 0x40U);
}
auto fake_texture(std::uintptr_t index) -> Texture* {
  return reinterpret_cast<Texture*>(0xE000U + index * 0x40U);
}

void fill_frame(DrawQueue& queue, std::size_t mesh_commands) {
  queue.clear();

  for (int chunk = 0; chunk < 16; ++chunk) {
    TerrainSurfaceCmd terrain;
    terrain.mesh = fake_mesh(static_cast<std::uintptr_t>(chunk));
    terrain.sort_key = static_cast<std::uint32_t>(chunk);
    queue.submit(terrain);
  }

  constexpr std::size_t k_archetypes = 12;
  for (std::size_t i = 0; i < mesh_commands; ++i) {
    const std::uintptr_t archetype = i % k_archetypes;
    MeshCmd mesh;
    mesh.mesh = fake_mesh(64U + archetype);
    mesh.shader = fake_shader(archetype % 3U);
    mesh.texture = fake_texture(archetype);
    mesh.material_id = static_cast<int>(archetype);
    queue.submit(mesh);
  }

  for (int marker = 0; marker < 64; ++marker) {
    queue.submit(GroundMarkerCmd{});
  }
}

auto time_sort_ms(DrawQueue& queue, std::size_t mesh_commands, int repeats) -> double {

  fill_frame(queue, mesh_commands);
  queue.sort_for_batching();

  std::vector<double> samples;
  samples.reserve(static_cast<std::size_t>(repeats));
  for (int i = 0; i < repeats; ++i) {
    fill_frame(queue, mesh_commands);
    const auto started = std::chrono::steady_clock::now();
    queue.sort_for_batching();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    samples.push_back(std::chrono::duration<double, std::milli>(elapsed).count());
  }

  std::sort(samples.begin(), samples.end());
  return samples[samples.size() / 2U];
}

TEST(DrawQueueSortCost, ARealisticFrameNeverSortsTheWholeQueue) {
  DrawQueue queue;
  fill_frame(queue, 2000);
  queue.sort_for_batching();

  ASSERT_EQ(queue.size(), 2000U + 16U + 64U);

  std::size_t terrain_end = 0;
  while (terrain_end < queue.size() &&
         queue.get_sorted(terrain_end).index() == Render::GL::TerrainSurfaceCmdIndex) {
    ++terrain_end;
  }
  EXPECT_EQ(terrain_end, 16U);

  std::size_t mesh_end = terrain_end;
  while (mesh_end < queue.size() &&
         queue.get_sorted(mesh_end).index() == Render::GL::MeshCmdIndex) {
    ++mesh_end;
  }
  EXPECT_EQ(mesh_end - terrain_end, 2000U);
  EXPECT_EQ(queue.size() - mesh_end, 64U);
}

TEST(DrawQueueSortCost, BatchingSurvivesTheSort) {
  DrawQueue queue;
  fill_frame(queue, 2400);
  queue.sort_for_batching();

  std::size_t instanced = 0;
  for (const auto& batch : queue.prepared_batches()) {
    if (batch.is_instanced()) {
      instanced += batch.count;
    }
  }
  EXPECT_GT(instanced, 2000U)
      << "sorting is supposed to earn its keep by producing instanceable runs";
}

TEST(DrawQueueSortCost, ReportsSortCostAcrossQueueSizes) {
  DrawQueue queue;

  const double at_2k = time_sort_ms(queue, 2000, 20);
  const double at_20k = time_sort_ms(queue, 20000, 20);
  const double at_100k = time_sort_ms(queue, 100000, 10);

  std::printf("draw queue sort (median of repeated frames):\n"
              "  2,080 commands  %.3f ms\n"
              " 20,080 commands  %.3f ms\n"
              "100,080 commands  %.3f ms\n",
              at_2k,
              at_20k,
              at_100k);

  EXPECT_GT(at_2k, 0.0);
  EXPECT_GT(at_20k, 0.0);
  EXPECT_GT(at_100k, 0.0);
}

} // namespace
