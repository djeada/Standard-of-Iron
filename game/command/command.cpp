#include "command.h"

namespace Game::Command {

auto source_name(Source source) -> const char* {
  switch (source) {
  case Source::LocalPlayer:
    return "local-player";
  case Source::AI:
    return "ai";
  case Source::Replay:
    return "replay";
  case Source::Script:
    return "script";
  }
  return "unknown";
}

auto payload_name(const Payload& payload) -> const char* {
  return std::visit(
      [](const auto& value) -> const char* {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Move>) {
          return "move";
        } else if constexpr (std::is_same_v<T, AttackTarget>) {
          return "attack-target";
        } else if constexpr (std::is_same_v<T, Stop>) {
          return "stop";
        } else if constexpr (std::is_same_v<T, SetHold>) {
          return "set-hold";
        } else if constexpr (std::is_same_v<T, SetGuard>) {
          return "set-guard";
        } else if constexpr (std::is_same_v<T, SetRunMode>) {
          return "set-run-mode";
        } else if constexpr (std::is_same_v<T, Patrol>) {
          return "patrol";
        } else if constexpr (std::is_same_v<T, SetRallyPoint>) {
          return "set-rally-point";
        } else if constexpr (std::is_same_v<T, SetGateMode>) {
          return "set-gate-mode";
        } else {
          return "produce";
        }
      },
      payload);
}

auto affected_units(const Payload& payload)
    -> const std::vector<Engine::Core::EntityID>* {
  return std::visit(
      [](const auto& value) -> const std::vector<Engine::Core::EntityID>* {
        using T = std::decay_t<decltype(value)>;
        if constexpr (requires { value.units; }) {
          return &value.units;
        } else {
          return nullptr;
        }
      },
      payload);
}

} // namespace Game::Command
