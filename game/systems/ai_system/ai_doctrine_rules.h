#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

[[nodiscard]] auto holds_garrison(const AIStrategyConfig& strategy) -> bool;
[[nodiscard]] auto can_initiate_attack(const AIStrategyConfig& strategy) -> bool;
[[nodiscard]] auto is_no_economy_nation(const AIContext& ctx) -> bool;

[[nodiscard]] auto reactive_attack_size(const AIStrategyConfig& strategy) -> int;
[[nodiscard]] auto proactive_attack_size(const AIStrategyConfig& strategy) -> int;

[[nodiscard]] auto ready_attack_force(const AIContext& ctx) -> int;
[[nodiscard]] auto committable_attack_force(const AIContext& ctx) -> int;

[[nodiscard]] auto
resume_attack_health_threshold(const AIStrategyConfig& strategy) -> float;
[[nodiscard]] auto
return_to_idle_health_threshold(const AIStrategyConfig& strategy) -> float;

[[nodiscard]] auto has_active_local_threat(const AIContext& ctx) -> bool;
[[nodiscard]] auto has_recent_local_threat(const AIContext& ctx,
                                           float game_time) -> bool;

} // namespace Game::Systems::AI
