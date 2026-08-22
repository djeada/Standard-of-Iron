#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "component_registry.h"

namespace Engine::Core {

enum class SystemPhase : std::uint8_t {

  Input = 0,

  Movement = 1,

  Combat = 2,

  Strategy = 3,

  Economy = 4,

  Ambient = 5,

  Presentation = 6,

  Cleanup = 7,

  _Count = 8
};

[[nodiscard]] auto phase_name(SystemPhase phase) noexcept -> const char*;

template <typename... Components>
struct Reads {};

template <typename... Components>
struct Writes {};

struct SystemAccess {
  std::vector<ComponentTypeId> reads;
  std::vector<ComponentTypeId> writes;
  bool exclusive = true;

  [[nodiscard]] static auto everything() -> SystemAccess { return SystemAccess{}; }

  template <typename... ReadTypes>
  [[nodiscard]] static auto reads_only() -> SystemAccess {
    return SystemAccess{
        .reads = {component_type_id<ReadTypes>()...}, .writes = {}, .exclusive = false};
  }

  template <typename... ReadTypes, typename... WriteTypes>
  [[nodiscard]] static auto declare(Reads<ReadTypes...>,
                                    Writes<WriteTypes...>) -> SystemAccess {
    return SystemAccess{.reads = {component_type_id<ReadTypes>()...},
                        .writes = {component_type_id<WriteTypes>()...},
                        .exclusive = false};
  }

  template <typename... WriteTypes>
  [[nodiscard]] static auto declare(Writes<WriteTypes...> writes) -> SystemAccess {
    return declare(Reads<>{}, writes);
  }

  template <typename... ReadTypes>
  [[nodiscard]] static auto declare(Reads<ReadTypes...> reads) -> SystemAccess {
    return declare(reads, Writes<>{});
  }

  [[nodiscard]] auto conflicts_with(const SystemAccess& other) const -> bool;
};

[[nodiscard]] auto plan_phase_batches(std::span<const SystemAccess> systems)
    -> std::vector<std::vector<std::size_t>>;

} // namespace Engine::Core
