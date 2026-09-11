#pragma once

#include <cstdint>
#include <string>

namespace Engine::Core {
class World;
}

namespace Game::Session {

class SessionContext;

struct SubsystemDigests {
  std::uint64_t identity = 0;
  std::uint64_t movement = 0;
  std::uint64_t combat = 0;
  std::uint64_t status = 0;
  std::uint64_t economy = 0;
  std::uint64_t wildlife = 0;
  std::uint64_t session = 0;
  std::uint64_t root = 0;
};

[[nodiscard]] auto world_digest(const Engine::Core::World& world) -> std::uint64_t;
[[nodiscard]] auto session_digest(SessionContext& session) -> std::uint64_t;

[[nodiscard]] auto subsystem_digests(SessionContext& session) -> SubsystemDigests;

[[nodiscard]] auto name_of_first_difference(const SubsystemDigests& recorded,
                                            const SubsystemDigests& observed) -> const
    char*;

[[nodiscard]] auto describe_world(const Engine::Core::World& world) -> std::string;

} // namespace Game::Session
