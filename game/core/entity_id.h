#pragma once

#include <cstdint>

namespace Engine::Core {

using EntityID = std::uint64_t;
constexpr EntityID NULL_ENTITY = 0;

namespace Handle {

constexpr unsigned k_index_bits = 32U;
constexpr EntityID k_index_mask = (EntityID{1} << k_index_bits) - 1U;

constexpr auto make(std::uint32_t index, std::uint32_t generation) -> EntityID {
  return (static_cast<EntityID>(generation) << k_index_bits) |
         static_cast<EntityID>(index);
}

constexpr auto index_of(EntityID id) -> std::uint32_t {
  return static_cast<std::uint32_t>(id & k_index_mask);
}

constexpr auto generation_of(EntityID id) -> std::uint32_t {
  return static_cast<std::uint32_t>(id >> k_index_bits);
}

} // namespace Handle

} // namespace Engine::Core
