#include "formation_slot_adjust.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "formation_frame.h"

namespace Game::Formation::planning {

namespace {

void resolve_overlaps(std::vector<FormationSlot>& slot_list,
                      const std::vector<float>& half_width,
                      const std::vector<float>& half_depth,
                      float gap) {
  constexpr int k_passes = 12;
  auto const count = slot_list.size();
  if (count < 2U || half_width.size() != count || half_depth.size() != count) {
    return;
  }

  for (int pass = 0; pass < k_passes; ++pass) {
    bool moved = false;
    for (std::size_t i = 0; i < count; ++i) {
      for (std::size_t j = i + 1; j < count; ++j) {
        float const dx = slot_list[j].local_offset.x() - slot_list[i].local_offset.x();
        float const dz = slot_list[j].local_offset.z() - slot_list[i].local_offset.z();
        float const need_x = half_width[i] + half_width[j] + gap;
        float const need_z = half_depth[i] + half_depth[j] + gap;
        float const over_x = need_x - std::abs(dx);
        float const over_z = need_z - std::abs(dz);
        if (over_x <= 0.0F || over_z <= 0.0F) {
          continue;
        }
        moved = true;

        if (over_x <= over_z) {
          float const push = (dx >= 0.0F ? over_x : -over_x) * 0.5F;
          slot_list[i].local_offset.setX(slot_list[i].local_offset.x() - push);
          slot_list[j].local_offset.setX(slot_list[j].local_offset.x() + push);
        } else {
          float const push = (dz >= 0.0F ? over_z : -over_z) * 0.5F;
          slot_list[i].local_offset.setZ(slot_list[i].local_offset.z() - push);
          slot_list[j].local_offset.setZ(slot_list[j].local_offset.z() + push);
        }
      }
    }
    if (!moved) {
      return;
    }
  }
}

void push_out_of_settled(FormationSlot& slot,
                         std::size_t index,
                         const std::vector<FormationSlot>& slot_list,
                         const std::vector<std::size_t>& settled,
                         const std::vector<float>& half_width,
                         const std::vector<float>& half_depth,
                         float gap) {
  for (std::size_t guard = 0; guard <= settled.size(); ++guard) {
    bool pushed = false;
    for (auto const other_index : settled) {
      const auto& other = slot_list[other_index];
      float const dx = slot.local_offset.x() - other.local_offset.x();
      float const dz = slot.local_offset.z() - other.local_offset.z();
      if (!local_overlap(dx,
                         dz,
                         half_width[index],
                         half_depth[index],
                         half_width[other_index],
                         half_depth[other_index],
                         gap)) {
        continue;
      }
      slot.local_offset.setZ(other.local_offset.z() -
                             (half_depth[index] + half_depth[other_index] + gap));
      pushed = true;
      break;
    }
    if (!pushed) {
      break;
    }
  }
}

auto front_to_back_order(const std::vector<FormationSlot>& slot_list)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> order(slot_list.size());
  for (std::size_t i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    if (slot_list[a].local_offset.z() != slot_list[b].local_offset.z()) {
      return slot_list[a].local_offset.z() > slot_list[b].local_offset.z();
    }
    return slot_list[a].local_offset.x() < slot_list[b].local_offset.x();
  });
  return order;
}

} // namespace

void separate_footprints(std::vector<FormationSlot>& slot_list,
                         const std::vector<float>& half_width,
                         const std::vector<float>& half_depth,
                         float gap) {
  auto const count = slot_list.size();
  if (count < 2U || half_width.size() != count || half_depth.size() != count) {
    return;
  }
  resolve_overlaps(slot_list, half_width, half_depth, gap);

  auto const order = front_to_back_order(slot_list);
  std::vector<std::size_t> settled;
  settled.reserve(count);
  for (auto const index : order) {
    push_out_of_settled(
        slot_list[index], index, slot_list, settled, half_width, half_depth, gap);
    settled.push_back(index);
  }
}

void recentre_on_centroid(std::vector<FormationSlot>& slot_list) {
  if (slot_list.empty()) {
    return;
  }
  float cx = 0.0F;
  float cz = 0.0F;
  for (const auto& slot : slot_list) {
    cx += slot.local_offset.x();
    cz += slot.local_offset.z();
  }
  cx /= static_cast<float>(slot_list.size());
  cz /= static_cast<float>(slot_list.size());
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() - cx);
    slot.local_offset.setY(0.0F);
    slot.local_offset.setZ(slot.local_offset.z() - cz);
  }
}

void recentre(std::vector<FormationSlot>& slot_list) {
  if (slot_list.empty()) {
    return;
  }
  Bounds bounds;
  for (const auto& slot : slot_list) {
    bounds.expand(slot.local_offset);
  }
  if (!bounds.valid) {
    return;
  }
  float const cx = (bounds.min_x + bounds.max_x) * 0.5F;
  float const cz = (bounds.min_z + bounds.max_z) * 0.5F;
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() - cx);
    slot.local_offset.setY(0.0F);
    slot.local_offset.setZ(slot.local_offset.z() - cz);
  }
}

void apply_reserve_rows(std::vector<FormationSlot>& slot_list,
                        int reserve_rows,
                        float spacing) {
  if (reserve_rows <= 0 || slot_list.empty()) {
    return;
  }

  std::vector<FormationSlot*> ordered;
  ordered.reserve(slot_list.size());
  for (auto& slot : slot_list) {
    ordered.push_back(&slot);
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
    return a->local_offset.z() < b->local_offset.z();
  });

  int bands = 0;
  float previous = ordered.front()->local_offset.z();
  std::size_t count = 0;
  for (auto* slot : ordered) {
    if (std::abs(slot->local_offset.z() - previous) > spacing * 0.5F) {
      ++bands;
      previous = slot->local_offset.z();
      if (bands >= reserve_rows) {
        break;
      }
    }
    ++count;
  }

  if (count >= ordered.size()) {
    return;
  }

  for (std::size_t i = 0; i < count; ++i) {
    auto* slot = ordered[i];
    slot->local_offset.setZ(slot->local_offset.z() - spacing * 1.15F);
    if (slot->role == ArmyRole::Centre || slot->role == ArmyRole::Vanguard ||
        slot->role == ArmyRole::Screen) {
      slot->role = ArmyRole::Reserve;
    }
  }
}

void apply_ranged_placement(std::vector<FormationSlot>& slot_list,
                            RangedPlacement placement,
                            float spacing) {
  Bounds body;
  Bounds ranged;
  for (const auto& slot : slot_list) {
    if (slot.role == ArmyRole::Ranged) {
      ranged.expand(slot.local_offset);
    } else if (slot.role == ArmyRole::Centre || slot.role == ArmyRole::Screen ||
               slot.role == ArmyRole::Vanguard) {
      body.expand(slot.local_offset);
    }
  }
  if (!ranged.valid || !body.valid) {
    return;
  }

  float shift = 0.0F;
  float lateral_spread = 1.0F;
  switch (placement) {
  case RangedPlacement::Front:

    shift = (body.max_z + spacing * 0.9F) - ranged.min_z;
    break;
  case RangedPlacement::Skirmish:

    shift = (body.max_z + spacing * 2.2F) - ranged.min_z;
    lateral_spread = 1.45F;
    break;
  case RangedPlacement::Rear:
  case RangedPlacement::Automatic:

    shift = (body.min_z - spacing * 0.9F) - ranged.max_z;
    break;
  }

  for (auto& slot : slot_list) {
    if (slot.role != ArmyRole::Ranged) {
      continue;
    }
    slot.local_offset.setZ(slot.local_offset.z() + shift);
    slot.local_offset.setX(slot.local_offset.x() * lateral_spread);
  }
}

void scale_to_frontage(std::vector<FormationSlot>& slot_list,
                       float requested_frontage,
                       float frontage_scale,
                       float depth_scale,
                       float lateral_floor,
                       float max_frontage,
                       float max_depth) {
  if (slot_list.empty()) {
    return;
  }
  Bounds bounds;
  for (const auto& slot : slot_list) {
    bounds.expand(slot.local_offset);
  }
  float lateral_scale = frontage_scale;
  if (requested_frontage > 0.01F && bounds.width() > 0.01F) {

    lateral_scale = std::max(1.0F, requested_frontage / bounds.width());
  }

  if (max_frontage > 0.1F && bounds.width() > 0.01F) {
    lateral_scale = std::min(lateral_scale, max_frontage / bounds.width());
  }
  lateral_scale = std::clamp(std::max(lateral_scale, lateral_floor), 0.25F, 6.0F);

  float depth_multiplier = depth_scale;
  if (max_depth > 0.1F && bounds.depth() > 0.01F) {
    depth_multiplier = std::min(depth_multiplier, max_depth / bounds.depth());
  }
  depth_multiplier = std::clamp(depth_multiplier, 1.0F, 6.0F);
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() * lateral_scale);
    slot.local_offset.setZ(slot.local_offset.z() * depth_multiplier);
  }
}

} // namespace Game::Formation::planning
