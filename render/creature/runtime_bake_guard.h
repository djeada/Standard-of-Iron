#pragma once

#include <cstdint>
#include <string_view>

#include "render_request.h"

namespace Render::Creature {

enum class RuntimeBakeOperation : std::uint8_t {
  RiggedMeshBake,
  SnapshotMeshBake,
  SnapshotMeshLoad,
  SkinAtlasBuild,
  SkinUboUpload,
  CreatureSubmitMiss,
  StaticArchetypeBuild
};

void set_runtime_bake_forbidden(bool forbidden) noexcept;
[[nodiscard]] auto runtime_bake_forbidden() noexcept -> bool;

class RuntimeBakeAllowScope {
public:
  RuntimeBakeAllowScope() noexcept;
  ~RuntimeBakeAllowScope();
  RuntimeBakeAllowScope(const RuntimeBakeAllowScope&) = delete;
  auto operator=(const RuntimeBakeAllowScope&) -> RuntimeBakeAllowScope& = delete;

private:
  bool m_previous{false};
};

void report_runtime_bake_violation(RuntimeBakeOperation operation,
                                   std::string_view detail);

[[nodiscard]] auto
runtime_bake_operation_name(RuntimeBakeOperation operation) -> std::string_view;

} // namespace Render::Creature
