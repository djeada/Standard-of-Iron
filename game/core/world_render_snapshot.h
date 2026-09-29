#pragma once

#include <cstdint>

namespace Engine::Core {

class Entity;

constexpr std::uint64_t k_render_signature_unstable = 0ULL;

auto render_entity_is_stable(const Entity& entity) -> bool;

auto render_entity_signature(const Entity& entity) -> std::uint64_t;

} // namespace Engine::Core
