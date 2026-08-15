#pragma once

#include <cstdint>
#include <string>

namespace Engine::Core {
class World;
}

namespace Game::Session {

class SessionContext;

[[nodiscard]] auto world_digest(const Engine::Core::World& world) -> std::uint64_t;
[[nodiscard]] auto session_digest(SessionContext& session) -> std::uint64_t;

[[nodiscard]] auto describe_world(const Engine::Core::World& world) -> std::string;

} // namespace Game::Session
