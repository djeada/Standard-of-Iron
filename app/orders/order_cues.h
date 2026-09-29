#pragma once

#include <cstdint>
#include <vector>

#include "game/audio/cue_ids.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Orders {

[[nodiscard]] auto selection_mounts(Engine::Core::World& world,
                                    const std::vector<Engine::Core::EntityID>& units)
    -> Game::Audio::Cue::SelectionMounts;

[[nodiscard]] auto charge_cue(const Game::Audio::Cue::SelectionMounts& mounts) -> const
    char*;

[[nodiscard]] auto
move_order_cue(const Game::Audio::Cue::SelectionMounts& mounts) -> const char*;

} // namespace App::Orders
