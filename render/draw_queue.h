#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <variant>
#include <vector>

#include "render/draw_cmd_traits.h"
#include "render/draw_commands.h"
#include "render/frame_budget.h"

namespace Render::GL {

enum class PreparedBatchKind : std::uint8_t {
  Single,
  CylinderInstanced,
  MeshInstanced,
  DrawPartInstanced,
  RiggedCreatureInstanced,
  GroundMarkerInstanced,
  EffectInstanced,
  ModeIndicatorInstanced
};

struct PreparedBatch {
  std::size_t start = 0;
  std::size_t count = 0;
  DrawCmdType type = DrawCmdType::Grid;
  PreparedBatchKind kind = PreparedBatchKind::Single;
  std::uint64_t sort_key = 0;

  [[nodiscard]] auto end() const noexcept -> std::size_t { return start + count; }

  [[nodiscard]] auto is_instanced() const noexcept -> bool {
    return kind != PreparedBatchKind::Single;
  }
};

class DrawQueue {
public:
  void clear();

  void submit_local_light(const LocalLight& light);

  [[nodiscard]] auto local_lights() const noexcept -> const std::vector<LocalLight>& {
    return m_local_lights;
  }

  void reserve_for_frame(std::size_t items_hint = 0);

  [[nodiscard]] auto items_high_water() const noexcept -> std::size_t {
    return m_items_high_water;
  }
  [[nodiscard]] auto prepared_high_water() const noexcept -> std::size_t {
    return m_prepared_high_water;
  }
  [[nodiscard]] auto submission_bucket_high_water() const noexcept -> std::size_t {
    return m_submission_bucket_high_water;
  }
  [[nodiscard]] auto submission_bucket_capacity() const noexcept -> std::size_t {
    return m_submission_bucket_spans.capacity();
  }

  template <typename CmdT,
            typename = std::enable_if_t<std::is_constructible_v<DrawCmd, CmdT&&>>>
  void submit(CmdT&& cmd) {
    DrawCmd draw_cmd(std::forward<CmdT>(cmd));
    record_submission_bucket(draw_cmd);
    m_items.emplace_back(std::move(draw_cmd));
  }

  [[nodiscard]] auto empty() const -> bool { return m_items.empty(); }
  [[nodiscard]] auto size() const -> std::size_t { return m_items.size(); }

  [[nodiscard]] auto get_sorted(std::size_t i) const -> const DrawCmd& {
    return m_items[m_sort_indices[i]];
  }

  [[nodiscard]] auto items() const -> const std::vector<DrawCmd>& { return m_items; }

  [[nodiscard]] auto prepared_batches() const -> const std::vector<PreparedBatch>& {
    return m_prepared_batches;
  }

  [[nodiscard]] auto sort_key_for_sorted(std::size_t i) const -> std::uint64_t {
    return m_sort_keys[m_sort_indices[i]];
  }

  void sort_for_batching();

  [[nodiscard]] auto can_batch_mesh(std::size_t sorted_idx_a,
                                    std::size_t sorted_idx_b) const -> bool;

private:
  struct SortIdentity {
    std::uint8_t pass = 0;
    std::uint8_t pipeline = 0;
    std::uint8_t transparency_bucket = 0;
    std::uint16_t material = 0;
    std::uint16_t mesh = 0;
    std::uint16_t texture = 0;
    std::uint8_t skeleton = 0;

    [[nodiscard]] auto pack() const noexcept -> std::uint64_t {
      return (static_cast<std::uint64_t>(pass) << 56) |
             (static_cast<std::uint64_t>(pipeline) << 48) |
             (static_cast<std::uint64_t>(transparency_bucket & 0x0FU) << 44) |
             (static_cast<std::uint64_t>(material & 0x0FFFU) << 32) |
             (static_cast<std::uint64_t>(mesh) << 16) |
             (static_cast<std::uint64_t>(texture & 0x0FFFU) << 4) |
             static_cast<std::uint64_t>(skeleton & 0x0FU);
    }
  };

  struct SubmissionBucketSpan {
    std::uint32_t bucket = 0;
    std::size_t start = 0;
    std::size_t count = 0;
    bool preserves_append_order = false;

    [[nodiscard]] auto end() const noexcept -> std::size_t { return start + count; }
  };

  enum class SortPipeline : std::uint8_t {
    Mesh = 0,
    DrawPart = 1,
    RiggedCreature = 2,
    Cylinder = 3,
    Fog = 4,
    TerrainScatterGrass = 8,
    TerrainScatterStone = 9,
    TerrainScatterPlant = 10,
    TerrainScatterPine = 11,
    TerrainScatterOlive = 12,
    TerrainScatterFireCamp = 13,
    Rain = 20,
    TerrainSurface = 21,
    TerrainFeatureWater = 22,
    TerrainFeatureRoad = 23,
    TerrainFeatureShoreline = 24,
    TerrainFeatureBridge = 25,
    PrimitiveSphere = 27,
    PrimitiveCylinder = 28,
    PrimitiveCone = 29,
    Effect = 30,
    Grid = 31,
    SelectionSmoke = 32,
    GroundMarker = 33,
    ModeIndicator = 34
  };

  void sort_full_keys(std::size_t start, std::size_t end);

  [[nodiscard]] auto sort_bucketed_ranges(std::size_t count) -> bool;

  void populate_sort_identity_prefix(const DrawCmd& cmd, SortIdentity& identity) const;

  [[nodiscard]] auto
  compute_submission_bucket(const DrawCmd& cmd) const -> std::uint32_t;

  [[nodiscard]] auto preserves_append_order(const DrawCmd& cmd) const -> bool;

  void record_submission_bucket(const DrawCmd& cmd);

  [[nodiscard]] auto compute_sort_key(const DrawCmd& cmd) -> uint64_t;

  void build_prepared_batches();

  [[nodiscard]] auto can_batch_draw_part(std::size_t sorted_idx_a,
                                         std::size_t sorted_idx_b) const -> bool;

  [[nodiscard]] auto can_batch_rigged(std::size_t sorted_idx_a,
                                      std::size_t sorted_idx_b) const -> bool;

  [[nodiscard]] auto can_batch_terrain_surface(std::size_t sorted_idx_a,
                                               std::size_t sorted_idx_b) const -> bool;

  [[nodiscard]] auto can_batch_terrain_feature(std::size_t sorted_idx_a,
                                               std::size_t sorted_idx_b) const -> bool;

  static auto transparency_bucket(float alpha) noexcept -> std::uint8_t {
    return alpha < k_opaque_threshold ? 1U : 0U;
  }

  static auto pack_4(std::uint32_t value) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(value, 0x0FU));
  }

  static auto pack_12(std::uint32_t value) noexcept -> std::uint16_t {
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(value, 0x0FFFU));
  }

  static auto pack_16(std::uint32_t value) noexcept -> std::uint16_t {
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(value, 0xFFFFU));
  }

  static auto sort_id(const void* ptr) noexcept -> std::uint32_t;

  [[nodiscard]] static auto ptr_value(const void* ptr) noexcept -> std::uintptr_t {
    return reinterpret_cast<std::uintptr_t>(ptr);
  }

  [[nodiscard]] static auto
  full_resource_identity(const DrawCmd& cmd) noexcept -> std::array<std::uintptr_t, 4>;

  std::vector<DrawCmd> m_items;
  std::vector<uint32_t> m_sort_indices;
  std::vector<uint64_t> m_sort_keys;
  std::vector<PreparedBatch> m_prepared_batches;
  std::vector<SubmissionBucketSpan> m_submission_bucket_spans;
  bool m_submission_bucket_ordered = true;
  std::size_t m_items_high_water = 0;
  std::size_t m_prepared_high_water = 0;
  std::size_t m_submission_bucket_high_water = 0;
  std::size_t m_local_light_high_water = 0;
  std::vector<LocalLight> m_local_lights;
};

} // namespace Render::GL
