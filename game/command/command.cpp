#include "command.h"

#include <type_traits>

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
        } else if constexpr (std::is_same_v<T, SetAutoGather>) {
          return "set-auto-gather";
        } else if constexpr (std::is_same_v<T, Produce>) {
          return "produce";
        } else if constexpr (std::is_same_v<T, Trade>) {
          return "trade";
        } else if constexpr (std::is_same_v<T, UseCommanderAbility>) {
          return "use-commander-ability";
        } else if constexpr (std::is_same_v<T, SetFormationMode>) {
          return "set-formation-mode";
        } else if constexpr (std::is_same_v<T, DeployFormation>) {
          return "deploy-formation";
        } else if constexpr (std::is_same_v<T, ReleaseFormation>) {
          return "release-formation";
        } else if constexpr (std::is_same_v<T, StartConstruction>) {
          return "start-construction";
        } else if constexpr (std::is_same_v<T, StartHarvest>) {
          return "start-harvest";
        } else if constexpr (std::is_same_v<T, DeliverCivilians>) {
          return "deliver-civilians";
        } else if constexpr (std::is_same_v<T, DivideSquads>) {
          return "divide-squads";
        } else if constexpr (std::is_same_v<T, MergeSquads>) {
          return "merge-squads";
        } else if constexpr (std::is_same_v<T, RepairStructure>) {
          return "repair-structure";
        } else if constexpr (std::is_same_v<T, DismantleStructure>) {
          return "dismantle-structure";
        } else if constexpr (std::is_same_v<T, PlaceWallPlan>) {
          return "place-wall-plan";
        } else {
          return "place-building";
        }
      },
      payload);
}

auto affected_units(const Payload& payload)
    -> const std::vector<Engine::Core::EntityID>* {
  return std::visit(
      [](const auto& value) -> const std::vector<Engine::Core::EntityID>* {
        if constexpr (requires { value.units; }) {
          return &value.units;
        } else {
          return nullptr;
        }
      },
      payload);
}

} // namespace Game::Command
