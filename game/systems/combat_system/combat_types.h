#pragma once

namespace Game::Systems::Combat {

namespace Constants {
inline constexpr float kEngagementCooldown = 0.5F;
inline constexpr float kIdealMeleeDistance = 0.6F;
inline constexpr float kMaxMeleeSeparation = 0.9F;
inline constexpr float kMeleePullFactor = 0.3F;
inline constexpr float kMeleePullSpeed = 5.0F;
inline constexpr float kMoveAmountFactor = 0.5F;
inline constexpr float kMinDistance = 0.001F;
inline constexpr float kMaxDisplacementPerFrame = 0.02F;
inline constexpr float kRangeMultiplierHold = 1.5F;
inline constexpr float kDamageMultiplierArcherHold = 1.5F;
inline constexpr float kDamageMultiplierSpearmanHold = 2.0F;
inline constexpr float kDamageMultiplierDefaultHold = 1.75F;
inline constexpr float kOptimalRangeFactor = 0.85F;
inline constexpr float kOptimalRangeBuffer = 0.5F;
inline constexpr float kNewCommandThreshold = 0.25F;
inline constexpr float kArrowSpreadMin = -0.25F;
inline constexpr float kArrowSpreadMax = 0.25F;
inline constexpr float kArrowVerticalSpreadFactor = 2.0F;
inline constexpr float kArrowDepthSpreadFactor = 1.8F;
inline constexpr float kArrowStartHeight = 0.6F;
inline constexpr float kArrowStartOffset = 0.35F;
inline constexpr float kArrowTargetOffset = 0.5F;
inline constexpr float kArrowSpeed = 14.0F;
} // namespace Constants

} // namespace Game::Systems::Combat
