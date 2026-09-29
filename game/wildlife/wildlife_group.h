#pragma once

#include <cstdint>

#include "wildlife_species.h"

namespace Game::Wildlife {

struct GroupState {
  std::uint16_t id{0U};
  Species species{Species::Sheep};
  float home_x{0.0F};
  float home_z{0.0F};
  float roam_radius{14.0F};
  int desired_size{0};
  float respawn_timer{0.0F};
  std::uint32_t rng_state{1U};
};

struct WildlifeStats {
  std::uint64_t near_thinks{0U};
  std::uint64_t far_thinks{0U};
  std::uint64_t dormant_skips{0U};
  std::uint64_t flee_events{0U};
  std::uint64_t hunt_events{0U};
  std::uint64_t bites{0U};
  std::uint64_t respawns{0U};
  std::uint64_t stall_releases{0U};

  void reset() noexcept { *this = WildlifeStats{}; }
};

} // namespace Game::Wildlife
