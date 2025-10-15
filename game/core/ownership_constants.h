#pragma once

namespace Game {
namespace Core {

constexpr int NEUTRAL_OWNER_ID = -1;

inline bool isNeutralOwner(int ownerId) { return ownerId == NEUTRAL_OWNER_ID; }

} // namespace Core
} // namespace Game
