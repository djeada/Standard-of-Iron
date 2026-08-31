#pragma once

#include <optional>

#include "app/world/world_feedback.h"

namespace Engine::Core {
class World;
class CombatHitEvent;
class WorldFeedbackEvent;
} // namespace Engine::Core

namespace App::Core {

[[nodiscard]] auto
combat_feedback_tick(Engine::Core::World& world,
                     const Engine::Core::CombatHitEvent& event,
                     FeedbackStyle style) -> std::optional<WorldFeedbackTick>;

[[nodiscard]] auto world_feedback_tick(Engine::Core::World& world,
                                       const Engine::Core::WorldFeedbackEvent& event)
    -> std::optional<WorldFeedbackTick>;

} // namespace App::Core
