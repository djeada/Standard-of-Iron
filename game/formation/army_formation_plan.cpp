#include <algorithm>
#include <cmath>
#include <functional>

#include "army_formation_planner.h"

namespace Game::Formation {

namespace {

constexpr float k_tolerated_displaced_share = 0.12F;

} // namespace

auto ArmyFormationPlan::slot_for(EntityID entity) const -> const FormationSlot* {
  auto it =
      std::find_if(slot_list.begin(), slot_list.end(), [entity](const auto& slot) {
        return slot.occupant == entity;
      });
  return it == slot_list.end() ? nullptr : &(*it);
}

auto ArmyFormationPlan::placed_count() const -> int {
  return static_cast<int>(
      std::count_if(slot_list.begin(), slot_list.end(), [](const auto& s) {
        return s.status != SlotStatus::Blocked;
      }));
}

auto ArmyFormationPlan::depth_bands() const -> std::vector<int> {
  std::vector<int> bands;
  if (slot_list.empty()) {
    return bands;
  }

  std::vector<float> depths;
  depths.reserve(slot_list.size());
  for (const auto& slot : slot_list) {
    depths.push_back(slot.local_offset.z());
  }
  std::sort(depths.begin(), depths.end(), std::greater<>());

  float const band_gap = std::max(slot_spacing, 0.2F) * 0.5F;
  float band_start = depths.front();
  int in_band = 0;
  for (float const depth : depths) {
    if (std::abs(depth - band_start) > band_gap) {
      bands.push_back(in_band);
      band_start = depth;
      in_band = 0;
    }
    ++in_band;
  }
  bands.push_back(in_band);
  return bands;
}

auto ArmyFormationPlan::rank_count() const -> int {
  return static_cast<int>(depth_bands().size());
}

auto ArmyFormationPlan::file_count() const -> int {
  auto const bands = depth_bands();
  return bands.empty() ? 0 : *std::max_element(bands.begin(), bands.end());
}

auto ArmyFormationPlan::keeps_shape() const -> bool {
  if (!valid) {
    return false;
  }
  auto const tolerated = static_cast<int>(
      std::floor(static_cast<float>(slot_list.size()) * k_tolerated_displaced_share));
  return blocked_count <= tolerated &&
         blocked_count + adjusted_count <= std::max(1, tolerated);
}

} // namespace Game::Formation
