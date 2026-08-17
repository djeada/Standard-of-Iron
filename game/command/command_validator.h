#pragma once

#include <string>

#include "command.h"

namespace Engine::Core {
class World;
}

namespace Game::Command {

enum class Rejection : std::uint8_t {
  None,
  NoOwner,
  NoSubjects,
  DeadTarget,
  FriendlyTarget,
  MissingBuilding,
  NotOwnedBuilding,
  NotPermittedForSource,
  MalformedPayload
};

struct Validation {
  Rejection rejection = Rejection::None;

  Command command;

  [[nodiscard]] auto accepted() const -> bool { return rejection == Rejection::None; }
};

[[nodiscard]] auto rejection_name(Rejection rejection) -> const char*;

[[nodiscard]] auto validate(Engine::Core::World& world,
                            const Command& command) -> Validation;

} // namespace Game::Command
