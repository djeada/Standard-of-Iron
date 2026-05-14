#include "runtime_bake_guard.h"

#include <atomic>

namespace Render::Creature {

namespace {
std::atomic_bool g_runtime_bake_forbidden{false};
}

void set_runtime_bake_forbidden(bool forbidden) noexcept {
  g_runtime_bake_forbidden.store(forbidden, std::memory_order_release);
}

auto runtime_bake_forbidden() noexcept -> bool {
  return g_runtime_bake_forbidden.load(std::memory_order_acquire);
}

RuntimeBakeAllowScope::RuntimeBakeAllowScope() noexcept
    : m_previous(runtime_bake_forbidden()) {
  set_runtime_bake_forbidden(false);
}

RuntimeBakeAllowScope::~RuntimeBakeAllowScope() {
  set_runtime_bake_forbidden(m_previous);
}

auto runtime_bake_operation_name(RuntimeBakeOperation operation) -> std::string_view {
  switch (operation) {
  case RuntimeBakeOperation::RiggedMeshBake:
    return "rigged_mesh_bake";
  case RuntimeBakeOperation::SnapshotMeshBake:
    return "snapshot_mesh_bake";
  case RuntimeBakeOperation::SnapshotMeshLoad:
    return "snapshot_mesh_load";
  case RuntimeBakeOperation::SkinAtlasBuild:
    return "skin_atlas_build";
  case RuntimeBakeOperation::SkinUboUpload:
    return "skin_ubo_upload";
  case RuntimeBakeOperation::CreatureSubmitMiss:
    return "creature_submit_miss";
  }
  return "unknown";
}

void report_runtime_bake_violation(RuntimeBakeOperation operation,
                                   std::string_view detail) {
  (void)operation;
  (void)detail;
}

} // namespace Render::Creature
