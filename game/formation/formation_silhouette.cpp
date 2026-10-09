#include "formation_silhouette.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>

#include "formation_battle_orders.h"
#include "formation_row_split.h"
#include "formation_silhouette_shapes.h"
#include "formation_slot_adjust.h"

namespace Game::Formation::planning {

namespace {

constexpr float k_lateral_gap_metres = 1.6F;
constexpr float k_rank_gap_metres = 2.0F;

using Indices = std::vector<std::size_t>;
using RowList = std::vector<Indices>;

auto is_wing(ArmyRole role) -> bool {
  return role == ArmyRole::LeftFlank || role == ArmyRole::RightFlank;
}

auto is_core_role(ArmyRole role) -> bool {
  return role == ArmyRole::Centre || role == ArmyRole::Screen ||
         role == ArmyRole::Vanguard;
}

struct Tier {
  ArmyRole role{ArmyRole::Centre};
  bool core{false};
  float mean_z{0.0F};
  Indices members;
};

struct FrontRow {
  float front_z{0.0F};
  float front_half_depth{0.0F};
  float front_half_width{0.0F};
  float widest_z{0.0F};
  float widest_half_width{0.0F};
};

class SilhouetteLayout {
public:
  SilhouetteLayout(std::vector<FormationSlot>& slot_list,
                   const SlotExtents& extents,
                   const SilhouetteParams& params)
      : m_slots(slot_list)
      , m_extents(extents)
      , m_params(params)
      , m_intent(params.intent) {
    float const gap_scale = std::clamp(params.options.spacing_scale, 0.5F, 2.5F);
    m_lateral_gap = std::max(k_lateral_gap_metres, params.spacing) * gap_scale;
    m_rank_gap = std::max(k_rank_gap_metres, params.spacing) * gap_scale;
  }

  void run() {
    if (place_battle_order(m_slots, m_extents, m_params, {m_lateral_gap, m_rank_gap})) {
      return;
    }
    partition_into_tiers();
    if (m_tiers.empty()) {
      return;
    }
    size_core_rows();
    if (m_intent == ArmyFormationIntent::Defensive && place_defensive_square()) {
      return;
    }
    auto placed_rows = rows_fitted_to_depth();
    auto const front = place_rows(placed_rows);
    place_wing(m_left_wing, -1.0F, front);
    place_wing(m_right_wing, 1.0F, front);
    if (m_intent == ArmyFormationIntent::Encirclement) {
      bend_into_crescent(m_slots, m_left_wing, m_right_wing, front.front_half_width);
    }
    recentre_on_centroid(m_slots);
  }

private:
  [[nodiscard]] auto half_width(std::size_t index) const -> float {
    return m_extents.half_width[index];
  }
  [[nodiscard]] auto half_depth(std::size_t index) const -> float {
    return m_extents.half_depth[index];
  }
  [[nodiscard]] auto z_of(std::size_t index) const -> float {
    return m_slots[index].local_offset.z();
  }
  [[nodiscard]] auto owns_rows() const -> bool {
    return intent_owns_its_rows(m_intent);
  }

  void partition_into_tiers();
  void sort_and_order_tiers();
  void size_core_rows();
  [[nodiscard]] auto wing_width(const Indices& wing) const -> float;
  void cap_core_rows_to_template_frontage();
  auto place_defensive_square() -> bool;
  [[nodiscard]] auto ranged_placement() const -> RangedPlacement;
  [[nodiscard]] auto build_rows(int capacity,
                                const std::vector<int>& core_sizes) const -> RowList;
  [[nodiscard]] auto rows_depth(const RowList& rows) const -> float;
  auto rows_fitted_to_depth() -> RowList;
  [[nodiscard]] auto row_half_depth_of(const Indices& row) const -> float;
  [[nodiscard]] auto row_bodies_width(const Indices& row) const -> float;
  [[nodiscard]] auto row_gap_for(const Indices& row) const -> float;
  auto place_rows(const RowList& rows) -> FrontRow;
  void place_wing(Indices& wing, float side, const FrontRow& front);

  std::vector<FormationSlot>& m_slots;
  const SlotExtents& m_extents;
  const SilhouetteParams& m_params;
  ArmyFormationIntent m_intent;
  float m_lateral_gap{0.0F};
  float m_rank_gap{0.0F};

  Indices m_left_wing;
  Indices m_right_wing;
  std::vector<Tier> m_tiers;

  float m_average_width{0.0F};
  int m_core_count{0};
  std::vector<int> m_core_rows;
  int m_row_capacity{1};
};

void SilhouetteLayout::partition_into_tiers() {
  bool const wings_apart = m_intent != ArmyFormationIntent::Column;
  for (std::size_t i = 0; i < m_slots.size(); ++i) {
    auto const role = m_slots[i].role;
    if (wings_apart && role == ArmyRole::LeftFlank) {
      m_left_wing.push_back(i);
      continue;
    }
    if (wings_apart && role == ArmyRole::RightFlank) {
      m_right_wing.push_back(i);
      continue;
    }
    bool const core = is_core_role(role) || is_wing(role);
    auto tier = std::find_if(m_tiers.begin(), m_tiers.end(), [&](const Tier& t) {
      return core ? t.core : (!t.core && t.role == role);
    });
    if (tier == m_tiers.end()) {
      m_tiers.push_back({role, core, 0.0F, {}});
      tier = std::prev(m_tiers.end());
    }
    tier->members.push_back(i);
  }
  sort_and_order_tiers();
}

void SilhouetteLayout::sort_and_order_tiers() {
  for (auto& tier : m_tiers) {
    float sum = 0.0F;
    for (auto const index : tier.members) {
      sum += z_of(index);
    }
    tier.mean_z =
        sum / static_cast<float>(std::max<std::size_t>(1U, tier.members.size()));
    std::stable_sort(
        tier.members.begin(), tier.members.end(), [&](std::size_t a, std::size_t b) {
          float const za = z_of(a);
          float const zb = z_of(b);
          if (std::abs(za - zb) > 0.25F) {
            return za > zb;
          }
          return m_slots[a].local_offset.x() < m_slots[b].local_offset.x();
        });
  }
  std::stable_sort(m_tiers.begin(), m_tiers.end(), [](const Tier& a, const Tier& b) {
    return a.mean_z > b.mean_z;
  });
  if (m_intent == ArmyFormationIntent::Assault &&
      m_params.options.ranged_placement == RangedPlacement::Automatic) {
    std::stable_partition(m_tiers.begin(), m_tiers.end(), [](const Tier& tier) {
      return tier.role != ArmyRole::Ranged;
    });
  }
}

void SilhouetteLayout::size_core_rows() {
  std::size_t counted = 0;
  for (const auto& tier : m_tiers) {
    for (auto const index : tier.members) {
      m_average_width += 2.0F * half_width(index) + m_lateral_gap;
      ++counted;
    }
  }
  m_average_width /= static_cast<float>(std::max<std::size_t>(1U, counted));

  for (const auto& tier : m_tiers) {
    if (tier.core) {
      m_core_count += static_cast<int>(tier.members.size());
    }
  }
  if (m_params.frontage > 0.01F && !owns_rows()) {
    int const per_row = std::max(
        1, static_cast<int>(std::floor(m_params.frontage / m_average_width)) + 1);
    m_core_rows = even_rows(m_core_count, (m_core_count + per_row - 1) / per_row);
  } else {
    bool const has_rear_tier = std::any_of(
        m_tiers.begin(), m_tiers.end(), [](const Tier& t) { return !t.core; });
    m_core_rows =
        silhouette_rows(m_intent, m_core_count, m_params.options, has_rear_tier);
  }
  for (int const size : m_core_rows) {
    m_row_capacity = std::max(m_row_capacity, size);
  }
  if (m_core_count == 0) {
    m_row_capacity = std::max(
        1,
        silhouette_rows(m_intent, static_cast<int>(m_slots.size()), m_params.options)
            .front());
  }
  cap_core_rows_to_template_frontage();
}

auto SilhouetteLayout::wing_width(const Indices& wing) const -> float {
  float width = 0.0F;
  for (std::size_t k = 0; k < wing.size(); k += 2U) {
    float column = half_width(wing[k]);
    if (k + 1U < wing.size()) {
      column = std::max(column, half_width(wing[k + 1U]));
    }
    width += 2.0F * column + m_lateral_gap;
  }
  return width;
}

void SilhouetteLayout::cap_core_rows_to_template_frontage() {
  auto const& tmpl = m_params.tmpl;
  if (!(tmpl.max_frontage > 0.1F && m_params.frontage <= 0.01F && !owns_rows())) {
    return;
  }
  float const core_room =
      std::max(m_average_width,
               tmpl.max_frontage - wing_width(m_left_wing) - wing_width(m_right_wing));
  int const fits = std::max(
      1, static_cast<int>(std::floor((core_room + m_lateral_gap) / m_average_width)));
  if (m_row_capacity > fits) {
    m_row_capacity = fits;
    m_core_rows = even_rows(m_core_count, (m_core_count + fits - 1) / fits);
  }
}

auto SilhouetteLayout::place_defensive_square() -> bool {
  Indices perimeter = m_left_wing;
  Indices inner;
  for (const auto& tier : m_tiers) {
    auto& target = tier.core ? perimeter : inner;
    target.insert(target.end(), tier.members.begin(), tier.members.end());
  }
  perimeter.insert(perimeter.end(), m_right_wing.begin(), m_right_wing.end());
  if (static_cast<int>(perimeter.size()) < k_hollow_square_minimum) {
    return false;
  }
  place_hollow_square(m_slots,
                      {perimeter, inner},
                      m_extents.half_width,
                      m_extents.half_depth,
                      m_lateral_gap,
                      m_rank_gap);
  recentre_on_centroid(m_slots);
  return true;
}

auto SilhouetteLayout::ranged_placement() const -> RangedPlacement {
  RangedPlacement placement = m_params.options.ranged_placement;
  if (placement == RangedPlacement::Automatic) {
    placement = m_intent == ArmyFormationIntent::Assault ? RangedPlacement::Rear
                                                         : m_params.tmpl.default_ranged;
  }
  return placement;
}

auto SilhouetteLayout::build_rows(int capacity,
                                  const std::vector<int>& core_sizes) const -> RowList {
  auto const placement = ranged_placement();
  RowList rows;
  bool core_placed = false;
  for (const auto& tier : m_tiers) {
    auto const count = static_cast<int>(tier.members.size());
    std::vector<int> const sizes =
        tier.core ? core_sizes : even_rows(count, (count + capacity - 1) / capacity);
    if (!tier.core && m_intent == ArmyFormationIntent::Defensive &&
        tier.role == ArmyRole::Reserve && !rows.empty()) {
      rows.emplace_back();
    }
    std::size_t cursor = 0;
    for (int const size : sizes) {
      Indices row(tier.members.begin() + static_cast<std::ptrdiff_t>(cursor),
                  tier.members.begin() + static_cast<std::ptrdiff_t>(cursor) + size);
      cursor += static_cast<std::size_t>(size);
      std::stable_sort(row.begin(), row.end(), [&](std::size_t a, std::size_t b) {
        return m_slots[a].local_offset.x() < m_slots[b].local_offset.x();
      });
      rows.push_back(std::move(row));
    }
    if (!tier.core && !core_placed && tier.role == ArmyRole::Ranged &&
        placement == RangedPlacement::Skirmish) {
      rows.emplace_back();
    }
    core_placed = core_placed || tier.core;
  }
  return rows;
}

auto SilhouetteLayout::row_half_depth_of(const Indices& row) const -> float {
  float row_half_depth = 0.0F;
  for (auto const index : row) {
    row_half_depth = std::max(row_half_depth, half_depth(index));
  }
  return row_half_depth;
}

auto SilhouetteLayout::rows_depth(const RowList& rows) const -> float {
  float depth = 0.0F;
  float previous = 0.0F;
  bool first_row = true;
  for (const auto& row : rows) {
    if (row.empty()) {
      depth += m_rank_gap + 2.0F * previous;
      continue;
    }
    float const row_half_depth = row_half_depth_of(row);
    if (!first_row) {
      depth += previous + m_rank_gap + row_half_depth;
    }
    first_row = false;
    previous = row_half_depth;
  }
  return depth;
}

auto SilhouetteLayout::rows_fitted_to_depth() -> RowList {
  auto placed_rows = build_rows(m_row_capacity, m_core_rows);
  int const total = static_cast<int>(m_slots.size());
  int wedge_growth = 1;
  while (m_params.tmpl.max_depth > 0.1F &&
         rows_depth(placed_rows) > m_params.tmpl.max_depth && m_row_capacity < total) {
    if (m_intent == ArmyFormationIntent::Assault && m_core_rows.size() > 2U) {
      m_core_rows = wedge_rows(m_core_count, ++wedge_growth);
      m_row_capacity = std::max(m_row_capacity, m_core_rows.back());
    } else {
      ++m_row_capacity;
      m_core_rows =
          even_rows(m_core_count, (m_core_count + m_row_capacity - 1) / m_row_capacity);
    }
    placed_rows = build_rows(m_row_capacity, m_core_rows);
  }
  return placed_rows;
}

auto SilhouetteLayout::row_bodies_width(const Indices& row) const -> float {
  float width = 0.0F;
  for (auto const index : row) {
    width += 2.0F * half_width(index);
  }
  return width;
}

auto SilhouetteLayout::row_gap_for(const Indices& row) const -> float {
  float gap = m_lateral_gap;
  if (m_params.frontage > 0.01F && row.size() > 1U && !owns_rows()) {
    float const outer = half_width(row.front()) + half_width(row.back());
    float const spaced_width =
        row_bodies_width(row) + m_lateral_gap * static_cast<float>(row.size() - 1U);
    float const bodies =
        spaced_width - m_lateral_gap * static_cast<float>(row.size() - 1U) - outer;
    gap = std::max(m_lateral_gap,
                   (m_params.frontage - bodies) / static_cast<float>(row.size() - 1U));
  }
  return gap;
}

auto SilhouetteLayout::place_rows(const RowList& rows) -> FrontRow {
  FrontRow front;
  float z = 0.0F;
  float previous_half_depth = 0.0F;
  bool first = true;
  int rank = 0;
  for (auto const& row : rows) {
    if (row.empty()) {
      z -= m_rank_gap + 2.0F * previous_half_depth;
      continue;
    }
    float const row_half_depth = row_half_depth_of(row);
    float const gap = row_gap_for(row);
    float const width =
        row_bodies_width(row) + gap * static_cast<float>(row.size() - 1U);
    if (!first) {
      z -= previous_half_depth + m_rank_gap + row_half_depth;
    }
    float x = -width * 0.5F;
    int file = 0;
    for (auto const index : row) {
      float const hw = half_width(index);
      m_slots[index].local_offset = QVector3D(x + hw, 0.0F, z);
      m_slots[index].rank = rank;
      m_slots[index].file = file++;
      x += 2.0F * hw + gap;
    }
    if (first) {
      front.front_z = z;
      front.front_half_depth = row_half_depth;
      front.front_half_width = width * 0.5F;
      first = false;
    }
    if (width * 0.5F > front.widest_half_width) {
      front.widest_half_width = width * 0.5F;
      front.widest_z = z;
    }
    previous_half_depth = row_half_depth;
    ++rank;
  }
  return front;
}

void SilhouetteLayout::place_wing(Indices& wing, float side, const FrontRow& front) {
  std::stable_sort(wing.begin(), wing.end(), [&](std::size_t a, std::size_t b) {
    return side * m_slots[a].local_offset.x() < side * m_slots[b].local_offset.x();
  });
  float const reach = m_intent == ArmyFormationIntent::Encirclement
                          ? front.front_half_depth * 2.0F + m_rank_gap
                          : 0.0F;
  bool const beside_widest = m_intent == ArmyFormationIntent::Assault;
  float const base_z = (beside_widest ? front.widest_z : front.front_z) + reach;
  float x = (beside_widest ? front.widest_half_width : front.front_half_width) +
            m_lateral_gap;
  int column = 0;
  float column_width = 0.0F;
  float wing_z = base_z;
  for (std::size_t k = 0; k < wing.size(); ++k) {
    auto& slot = m_slots[wing[k]];
    float const hw = half_width(wing[k]);
    float const hd = half_depth(wing[k]);
    column_width = std::max(column_width, hw);
    slot.local_offset = QVector3D(side * (x + hw), 0.0F, wing_z);
    slot.rank = column;
    wing_z -= 2.0F * hd + m_rank_gap;
    if ((k + 1U) % 2U == 0U) {
      x += 2.0F * column_width + m_lateral_gap;
      column_width = 0.0F;
      wing_z = base_z;
      ++column;
    }
  }
}

} // namespace

void regularize_silhouette(std::vector<FormationSlot>& slot_list,
                           const SlotExtents& extents,
                           const SilhouetteParams& params) {
  if (slot_list.size() < 2U) {
    return;
  }
  SilhouetteLayout(slot_list, extents, params).run();
}

} // namespace Game::Formation::planning
