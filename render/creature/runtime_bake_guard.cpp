#include "runtime_bake_guard.h"

#include <atomic>

#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include <QDebug>

#include <mutex>
#include <string>
#include <unordered_set>
#endif

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
  case RuntimeBakeOperation::StaticArchetypeBuild:
    return "static_archetype_build";
  }
  return "unknown";
}

void report_runtime_bake_violation(RuntimeBakeOperation operation,
                                   std::string_view detail) {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  static std::mutex mutex;
  static std::unordered_set<std::string> reported;
  const std::string key =
      std::string(runtime_bake_operation_name(operation)) + ":" + std::string(detail);
  std::lock_guard<std::mutex> const lock(mutex);
  if (reported.emplace(key).second) {
    qCritical().noquote() << "Forbidden render-time bake:"
                          << QString::fromStdString(key);
  }
#else
  (void)operation;
  (void)detail;
#endif
}

} // namespace Render::Creature
