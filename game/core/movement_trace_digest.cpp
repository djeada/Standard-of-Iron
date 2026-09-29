#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>

#include "movement_trace_analysis.h"

namespace Engine::Core {

auto movement_digest(const std::vector<MovementTroopSample>& troops,
                     const std::vector<MovementSoldierSample>& soldiers)
    -> std::string {

  std::uint64_t hash = 1469598103934665603ULL;
  auto mix = [&hash](std::uint64_t value) {
    for (int byte = 0; byte < 8; ++byte) {
      hash ^= (value >> (byte * 8)) & 0xFFULL;
      hash *= 1099511628211ULL;
    }
  };
  auto mix_position = [&mix](float value) {
    mix(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(std::llround(value * 1000.0F))));
  };

  std::vector<const MovementTroopSample*> ordered;
  ordered.reserve(troops.size());
  for (auto const& sample : troops) {
    ordered.push_back(&sample);
  }
  std::stable_sort(ordered.begin(),
                   ordered.end(),
                   [](const MovementTroopSample* lhs, const MovementTroopSample* rhs) {
                     if (lhs->tick != rhs->tick) {
                       return lhs->tick < rhs->tick;
                     }
                     return lhs->entity_id < rhs->entity_id;
                   });

  for (auto const* sample : ordered) {
    mix(sample->tick);
    mix(sample->entity_id);
    mix(static_cast<std::uint64_t>(sample->state));
    mix(sample->command_sequence);
    mix(sample->route_id);
    mix(sample->route_revision);
    mix_position(sample->lane_offset);
    mix_position(sample->lane_scale);
    mix_position(sample->cohesion_pace);
    mix(sample->portal_id);
    mix(static_cast<std::uint64_t>(sample->traversal_mode));
    mix(sample->current_files);
    mix_position(sample->root_x);
    mix_position(sample->root_z);
  }

  std::vector<const MovementSoldierSample*> ordered_soldiers;
  ordered_soldiers.reserve(soldiers.size());
  for (auto const& sample : soldiers) {
    ordered_soldiers.push_back(&sample);
  }
  std::stable_sort(
      ordered_soldiers.begin(),
      ordered_soldiers.end(),
      [](const MovementSoldierSample* lhs, const MovementSoldierSample* rhs) {
        if (lhs->frame != rhs->frame) {
          return lhs->frame < rhs->frame;
        }
        if (lhs->troop_id != rhs->troop_id) {
          return lhs->troop_id < rhs->troop_id;
        }
        return lhs->stable_slot < rhs->stable_slot;
      });
  for (auto const* sample : ordered_soldiers) {
    mix(sample->troop_id);
    mix(sample->stable_slot);
    mix(static_cast<std::uint64_t>(sample->traversal_mode));
  }

  std::array<char, 24> buffer{};
  std::snprintf(
      buffer.data(), buffer.size(), "%016llx", static_cast<unsigned long long>(hash));
  return std::string(buffer.data());
}

auto load_movement_trace_directory(const std::string& directory,
                                   std::vector<MovementTroopSample>& troops,
                                   std::vector<MovementSoldierSample>& soldiers)
    -> bool {
  std::ifstream troop_stream(directory + "/troops.jsonl");
  if (!troop_stream.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(troop_stream, line)) {
    if (line.empty()) {
      continue;
    }
    MovementTroopSample sample;
    if (parse_troop_sample(line, sample)) {
      troops.push_back(sample);
    }
  }

  std::ifstream soldier_stream(directory + "/soldiers.jsonl");
  if (soldier_stream.is_open()) {
    while (std::getline(soldier_stream, line)) {
      if (line.empty()) {
        continue;
      }
      MovementSoldierSample sample;
      if (parse_soldier_sample(line, sample)) {
        soldiers.push_back(sample);
      }
    }
  }
  return true;
}

} // namespace Engine::Core
