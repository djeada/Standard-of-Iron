#pragma once

#include <cstdint>

namespace Engine::Core::RenderPhase {

namespace Detail {
inline thread_local int g_render_depth = 0;
inline thread_local int g_handoff_depth = 0;
} // namespace Detail

class Scope {
public:
  Scope() noexcept { ++Detail::g_render_depth; }
  ~Scope() { --Detail::g_render_depth; }
  Scope(const Scope&) = delete;
  Scope(Scope&&) = delete;
  auto operator=(const Scope&) -> Scope& = delete;
  auto operator=(Scope&&) -> Scope& = delete;
};

class SnapshotHandoff {
public:
  SnapshotHandoff() noexcept { ++Detail::g_handoff_depth; }
  ~SnapshotHandoff() { --Detail::g_handoff_depth; }
  SnapshotHandoff(const SnapshotHandoff&) = delete;
  SnapshotHandoff(SnapshotHandoff&&) = delete;
  auto operator=(const SnapshotHandoff&) -> SnapshotHandoff& = delete;
  auto operator=(SnapshotHandoff&&) -> SnapshotHandoff& = delete;
};

[[nodiscard]] inline auto live_world_forbidden() noexcept -> bool {
  return Detail::g_render_depth > 0 && Detail::g_handoff_depth == 0;
}

void report_violation(const char* accessor) noexcept;

[[nodiscard]] auto violation_count() noexcept -> std::uint64_t;

} // namespace Engine::Core::RenderPhase
