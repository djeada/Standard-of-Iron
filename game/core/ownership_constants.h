#pragma once

namespace Game::Core {

constexpr int NEUTRAL_OWNER_ID = -1;

inline auto isNeutralOwner(int owner_id) -> bool {
  return owner_id == NEUTRAL_OWNER_ID;
}

} 
