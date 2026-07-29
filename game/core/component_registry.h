#pragma once

#include <cstddef>
#include <cstdint>
#include <typeindex>

namespace Engine::Core {

using ComponentTypeId = std::uint32_t;

auto resolve_component_type_id(std::type_index type) -> ComponentTypeId;

auto component_type_count() -> std::size_t;

template <typename T>
auto component_type_id() -> ComponentTypeId {
  static const ComponentTypeId id =
      resolve_component_type_id(std::type_index(typeid(T)));
  return id;
}

} // namespace Engine::Core
