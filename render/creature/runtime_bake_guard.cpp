#include "runtime_bake_guard.h"

#include <QDebug>
#include <QLatin1Char>
#include <QString>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>

#include "render/profiling/asset_counters.h"

namespace Render::Creature {

namespace {
std::atomic_bool g_runtime_bake_forbidden{false};
}

void set_runtime_bake_forbidden(bool forbidden) noexcept {
  const bool previous =
      g_runtime_bake_forbidden.exchange(forbidden, std::memory_order_acq_rel);
  if (forbidden == previous) {
    return;
  }
  if (forbidden) {
    Render::Profiling::asset_counters().mark_load_barrier();
  } else {
    Render::Profiling::asset_counters().clear_load_barrier();
  }
}

auto runtime_bake_forbidden() noexcept -> bool {
  return g_runtime_bake_forbidden.load(std::memory_order_acquire);
}

RuntimeBakeAllowScope::RuntimeBakeAllowScope() noexcept
    : m_previous(g_runtime_bake_forbidden.exchange(false, std::memory_order_acq_rel)) {
}

RuntimeBakeAllowScope::~RuntimeBakeAllowScope() {
  g_runtime_bake_forbidden.store(m_previous, std::memory_order_release);
}

auto runtime_bake_operation_name(RuntimeBakeOperation operation) -> std::string_view {
  switch (operation) {
  case RuntimeBakeOperation::RiggedMeshBake:
    return "rigged_mesh_bake";
  case RuntimeBakeOperation::SnapshotMeshBake:
    return "snapshot_mesh_bake";
  case RuntimeBakeOperation::SkinUboUpload:
    return "skin_ubo_upload";
  case RuntimeBakeOperation::CreatureSubmitMiss:
    return "creature_submit_miss";
  case RuntimeBakeOperation::StaticArchetypeBuild:
    return "static_archetype_build";
  }
  return "unknown";
}

namespace {

auto reported_identities_mutex() -> std::mutex& {
  static std::mutex mutex;
  return mutex;
}

auto reported_identities() -> std::unordered_set<std::uint64_t>& {
  static std::unordered_set<std::uint64_t> reported;
  return reported;
}

} // namespace

auto note_runtime_bake_violation(RuntimeBakeOperation operation,
                                 std::uint64_t identity) -> bool {
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::ForbiddenBake);
  const std::uint64_t key =
      identity ^ (static_cast<std::uint64_t>(operation) * 0x9E3779B97F4A7C15ULL);
  std::lock_guard<std::mutex> const lock(reported_identities_mutex());
  return reported_identities().emplace(key).second;
}

void log_runtime_bake_violation(RuntimeBakeOperation operation,
                                std::string_view detail) {
  const auto name = runtime_bake_operation_name(operation);
  qCritical().noquote()
      << "Forbidden render-time bake:"
      << (QString::fromUtf8(name.data(), static_cast<int>(name.size())) +
          QLatin1Char(':') +
          QString::fromUtf8(detail.data(), static_cast<int>(detail.size())));
}

void report_runtime_bake_violation(RuntimeBakeOperation operation,
                                   std::string_view detail) {
  static std::mutex mutex;
  static std::unordered_set<std::string> reported;
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::ForbiddenBake);
  const std::string key =
      std::string(runtime_bake_operation_name(operation)) + ":" + std::string(detail);
  std::lock_guard<std::mutex> const lock(mutex);
  if (reported.emplace(key).second) {
    qCritical().noquote() << "Forbidden render-time bake:"
                          << QString::fromStdString(key);
  }
}

void report_missing_preloaded_asset(std::string_view detail) {
  static std::mutex mutex;
  static std::unordered_set<std::string> reported;
  Render::Profiling::count_asset(
      Render::Profiling::AssetCounter::MissingPreloadedAsset);
  const std::string key(detail);
  {
    std::lock_guard<std::mutex> const lock(mutex);
    if (!reported.emplace(key).second) {
      return;
    }
  }
  const auto message = QString::fromStdString(key);
  if (runtime_bake_forbidden()) {
    qCritical().noquote() << "Missing preloaded creature asset:" << message;
  } else {
    qWarning().noquote() << "Missing preloaded creature asset:" << message;
  }
}

} // namespace Render::Creature
