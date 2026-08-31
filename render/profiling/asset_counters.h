#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Render::Profiling {

enum class AssetCounter : std::uint8_t {
  RiggedMeshBake = 0,
  SnapshotMeshBake,
  RiggedMeshConstructed,
  RiggedMeshVertexBytes,
  RiggedCacheHit,
  RiggedCacheMiss,
  SnapshotCacheHit,
  SnapshotCacheMiss,
  SnapshotLoad,
  SnapshotEviction,
  SkinAtlasBuild,
  SkinUboUpload,
  SkinUboBytes,
  ForbiddenBake,
  MissingPreloadedAsset,
  GlBufferCreated,
  GlVertexArrayCreated,
  GlTextureCreated,
  GlProgramCreated,
  ShaderCompiled,
  ProgramLinked,
  GlUploadBytes,
  RenderThreadAllocations,
  RenderThreadAllocatedBytes,
  PrewarmItemsQueued,
  PrewarmItemsProcessed,
  PrewarmInvocations,
  PrewarmSkippedNoRegistry,
  _Count
};

[[nodiscard]] auto
asset_counter_name(AssetCounter counter) noexcept -> std::string_view;

class AssetCounters {
public:
  static constexpr std::size_t k_count = static_cast<std::size_t>(AssetCounter::_Count);

  void add(AssetCounter counter, std::uint64_t amount = 1) noexcept {
    m_total[static_cast<std::size_t>(counter)].fetch_add(amount,
                                                         std::memory_order_relaxed);
  }

  [[nodiscard]] auto total(AssetCounter counter) const noexcept -> std::uint64_t {
    return m_total[static_cast<std::size_t>(counter)].load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto
  since_barrier(AssetCounter counter) const noexcept -> std::uint64_t {
    const std::size_t index = static_cast<std::size_t>(counter);
    const std::uint64_t now = m_total[index].load(std::memory_order_relaxed);
    const std::uint64_t at = m_at_barrier[index].load(std::memory_order_relaxed);
    return now > at ? now - at : 0U;
  }

  void mark_load_barrier() noexcept;

  void clear_load_barrier() noexcept;

  [[nodiscard]] auto load_barrier_marked() const noexcept -> bool {
    return m_barrier_marked.load(std::memory_order_acquire);
  }

  void reset() noexcept;

  [[nodiscard]] auto post_barrier_asset_work() const noexcept -> std::uint64_t;

private:
  std::array<std::atomic<std::uint64_t>, k_count> m_total{};
  std::array<std::atomic<std::uint64_t>, k_count> m_at_barrier{};
  std::atomic_bool m_barrier_marked{false};
};

[[nodiscard]] auto asset_counters() noexcept -> AssetCounters&;

inline void count_asset(AssetCounter counter, std::uint64_t amount = 1) noexcept {
  asset_counters().add(counter, amount);
}

[[nodiscard]] auto format_post_barrier_violations() -> std::string;

} // namespace Render::Profiling
