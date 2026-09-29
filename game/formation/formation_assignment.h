#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "army_formation_types.h"

namespace Game::Formation::planning {

[[nodiscard]] auto
min_cost_assignment(const std::vector<std::vector<float>>& cost) -> std::vector<int>;

struct NearestAssignmentInput {
  const std::vector<QVector3D>& slot_start;
  const std::vector<std::uint64_t>& slot_kind;
  QVector3D anchor;
  float facing{0.0F};
};

void assign_nearest_troops(std::vector<FormationSlot>& slot_list,
                           std::vector<int>& slot_files,
                           const NearestAssignmentInput& input);

void fold_onto_reference(std::vector<FormationSlot>& slot_list,
                         const std::vector<FormationSlot>& reference);

void keep_previous_occupants(std::vector<FormationSlot>& slot_list,
                             const std::vector<std::uint64_t>& slot_keys,
                             const std::vector<FormationSlot>& previous_slots);

} // namespace Game::Formation::planning
