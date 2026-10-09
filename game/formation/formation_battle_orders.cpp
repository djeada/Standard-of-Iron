#include "formation_battle_orders.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "formation_slot_adjust.h"

namespace Game::Formation::planning {

namespace {

using Indices = std::vector<std::size_t>;

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;
constexpr int k_crescent_centre_per_rank = 8;
constexpr float k_crescent_turn_share = 0.6F;
constexpr float k_screen_rank_gaps = 3.0F;
constexpr float k_elephant_screen_rank_gaps = 4.0F;
constexpr float k_elephant_min_gap = 8.0F;
constexpr float k_elephant_screen_span_share = 0.85F;

enum class Kind : std::uint8_t {
  Infantry,
  Cavalry,
  Elephant,
  Screen,
  Other
};

auto mask_of(std::initializer_list<RoleTag> tags) -> RoleTagSet {
  RoleTagSet set = 0U;
  for (auto tag : tags) {
    set |= to_mask(tag);
  }
  return set;
}

auto kind_of(RoleTagSet roles) -> Kind {
  if (has_role(roles, RoleTag::Elephant)) {
    return Kind::Elephant;
  }
  if (has_any_role(roles, mask_of({RoleTag::Cavalry, RoleTag::Mounted}))) {
    return Kind::Cavalry;
  }
  if (has_role(roles, RoleTag::Siege)) {
    return Kind::Other;
  }
  if (has_role(roles, RoleTag::Skirmisher) || has_role(roles, RoleTag::Ranged)) {
    return Kind::Screen;
  }
  if (has_any_role(roles,
                   mask_of({RoleTag::LineInfantry,
                            RoleTag::HeavyInfantry,
                            RoleTag::SpearInfantry,
                            RoleTag::Shielded}))) {
    return Kind::Infantry;
  }
  return Kind::Other;
}

struct Partition {
  Indices infantry;
  Indices cavalry;
  Indices elephants;
  Indices screen;
  Indices other;
};

class BattleOrderLayout {
public:
  BattleOrderLayout(std::vector<FormationSlot>& slot_list,
                    const SlotExtents& extents,
                    const SilhouetteParams& params,
                    const BattleOrderGaps& gaps)
      : m_slots(slot_list)
      , m_extents(extents)
      , m_params(params)
      , m_gaps(gaps)
      , m_gap_scale(std::clamp(params.options.spacing_scale, 0.5F, 2.5F)) {}

  auto run() -> bool {
    switch (m_params.intent) {
    case ArmyFormationIntent::TriplexAcies:
      place_triplex_acies();
      break;
    case ArmyFormationIntent::ConvexCrescent:
      place_convex_crescent();
      break;
    case ArmyFormationIntent::ElephantScreen:
      place_elephant_screen();
      break;
    default:
      return false;
    }
    recentre_on_centroid(m_slots);
    return true;
  }

private:
  [[nodiscard]] auto hw(std::size_t i) const -> float {
    return i < m_extents.half_width.size() ? m_extents.half_width[i] : 0.5F;
  }
  [[nodiscard]] auto hd(std::size_t i) const -> float {
    return i < m_extents.half_depth.size() ? m_extents.half_depth[i] : 0.5F;
  }
  [[nodiscard]] auto roles(std::size_t i) const -> RoleTagSet {
    return i < m_extents.roles.size() ? m_extents.roles[i] : 0U;
  }
  [[nodiscard]] auto allied(std::size_t i) const -> bool {
    return i < m_extents.allied.size() && m_extents.allied[i] != 0U;
  }
  [[nodiscard]] auto troop(std::size_t i) const -> int {
    return i < m_extents.troop.size() ? m_extents.troop[i] : -1;
  }

  [[nodiscard]] auto widest(const Indices& set) const -> float {
    float width = 0.0F;
    for (auto const i : set) {
      width = std::max(width, 2.0F * hw(i));
    }
    return width;
  }
  [[nodiscard]] auto deepest(const Indices& set) const -> float {
    float depth = 0.0F;
    for (auto const i : set) {
      depth = std::max(depth, 2.0F * hd(i));
    }
    return depth;
  }

  // Lateral order as the doctrine's line layout left it, so troops keep their
  // side of the army and the Hungarian assignment has little to undo.
  void sort_by_lateral(Indices& set) const {
    std::stable_sort(set.begin(), set.end(), [&](std::size_t a, std::size_t b) {
      float const xa = m_slots[a].local_offset.x();
      float const xb = m_slots[b].local_offset.x();
      if (std::abs(xa - xb) > 0.01F) {
        return xa < xb;
      }
      return a < b;
    });
  }

  auto partition() const -> Partition {
    Partition parts;
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
      switch (kind_of(roles(i))) {
      case Kind::Infantry:
        parts.infantry.push_back(i);
        break;
      case Kind::Cavalry:
        parts.cavalry.push_back(i);
        break;
      case Kind::Elephant:
        parts.elephants.push_back(i);
        break;
      case Kind::Screen:
        parts.screen.push_back(i);
        break;
      case Kind::Other:
        parts.other.push_back(i);
        break;
      }
    }
    sort_by_lateral(parts.infantry);
    sort_by_lateral(parts.cavalry);
    sort_by_lateral(parts.elephants);
    sort_by_lateral(parts.screen);
    sort_by_lateral(parts.other);
    return parts;
  }

  void put(std::size_t index,
           float x,
           float z,
           BattleBand band,
           int rank,
           int file,
           float local_facing = 0.0F) {
    auto& slot = m_slots[index];
    slot.local_offset = QVector3D(x, 0.0F, z);
    slot.local_facing = local_facing;
    slot.band = band;
    slot.rank = rank;
    slot.file = file;
    slot.yield_depth = 0.0F;
    slot.manoeuvre_offset = QVector3D();
    slot.manoeuvre_facing = 0.0F;
  }

  // A loose row centred on x = 0 at depth z (its centre line), spread over at
  // least `span` between the outermost troop centres.
  void place_row(const Indices& row,
                 float z_centre,
                 float span,
                 BattleBand band,
                 int rank) {
    if (row.empty()) {
      return;
    }
    float const min_pitch = widest(row) + m_gaps.lateral;
    float const pitch =
        row.size() > 1U
            ? std::max(min_pitch, span / static_cast<float>(row.size() - 1U))
            : 0.0F;
    auto const xs = lattice_positions(static_cast<int>(row.size()),
                                      row.size() % 2U == 0U ? 0.5F : 0.0F,
                                      std::max(pitch, 0.01F));
    for (std::size_t k = 0; k < row.size(); ++k) {
      put(row[k], xs[k], z_centre, band, rank, static_cast<int>(k));
    }
  }

  // Wings stand in columns two troops deep, from `inner_x` outward, their front
  // rank level with `front_z`. The first member stands innermost.
  void place_wing(const Indices& wing,
                  float side,
                  float inner_x,
                  float front_z,
                  BattleBand band,
                  int rank) {
    float x = inner_x;
    int column = 0;
    for (std::size_t k = 0; k < wing.size(); k += 2U) {
      std::size_t const front = wing[k];
      bool const has_back = k + 1U < wing.size();
      float column_half = hw(front);
      if (has_back) {
        column_half = std::max(column_half, hw(wing[k + 1U]));
      }
      float const centre_x = side * (x + column_half);
      put(front, centre_x, front_z - hd(front), band, rank, column);
      if (has_back) {
        std::size_t const back = wing[k + 1U];
        put(back,
            centre_x,
            front_z - 2.0F * hd(front) - m_gaps.rank - hd(back),
            band,
            rank + 1,
            column);
      }
      x += 2.0F * column_half + m_gaps.lateral;
      ++column;
    }
  }

  void split_sides(const Indices& set, Indices& left, Indices& right) const {
    std::size_t const left_count = set.size() / 2U;
    left.assign(set.begin(), set.begin() + static_cast<std::ptrdiff_t>(left_count));
    right.assign(set.begin() + static_cast<std::ptrdiff_t>(left_count), set.end());
    // Columns grow outward, so the left wing is filled from its inner end.
    std::reverse(left.begin(), left.end());
  }

  [[nodiscard]] auto span_of(const Indices& set) const -> float {
    float lo = 0.0F;
    float hi = 0.0F;
    bool any = false;
    for (auto const i : set) {
      float const x = m_slots[i].local_offset.x();
      lo = any ? std::min(lo, x - hw(i)) : x - hw(i);
      hi = any ? std::max(hi, x + hw(i)) : x + hw(i);
      any = true;
    }
    return any ? std::max(std::abs(lo), std::abs(hi)) : 0.0F;
  }

  [[nodiscard]] auto front_of(const Indices& set) const -> float {
    float front = 0.0F;
    bool any = false;
    for (auto const i : set) {
      float const z = m_slots[i].local_offset.z() + hd(i);
      front = any ? std::max(front, z) : z;
      any = true;
    }
    return front;
  }

  [[nodiscard]] auto rear_of(const Indices& set) const -> float {
    float rear = 0.0F;
    bool any = false;
    for (auto const i : set) {
      float const z = m_slots[i].local_offset.z() - hd(i);
      rear = any ? std::min(rear, z) : z;
      any = true;
    }
    return rear;
  }

  void place_rear_row(const Indices& rest, float behind_z, int rank) {
    if (rest.empty()) {
      return;
    }
    float const depth = deepest(rest);
    place_row(
        rest, behind_z - 2.0F * m_gaps.rank - depth * 0.5F, 0.0F, BattleBand::None, rank);
  }

  void place_triplex_acies();
  void place_convex_crescent();
  void place_elephant_screen();

  std::vector<FormationSlot>& m_slots;
  const SlotExtents& m_extents;
  const SilhouetteParams& m_params;
  BattleOrderGaps m_gaps;
  float m_gap_scale{1.0F};
};

// Hastati in front, principes behind them standing on the hastati gaps (the
// quincunx), triarii behind the principes on the hastati files again. Every
// maniple of a line is separated from its neighbour by an open lane at least as
// wide as a maniple.
void BattleOrderLayout::place_triplex_acies() {
  auto parts = partition();
  auto const n = static_cast<int>(parts.infantry.size());

  int triarii = 0;
  int principes = 0;
  if (n >= 3) {
    int const spears = static_cast<int>(
        std::count_if(parts.infantry.begin(), parts.infantry.end(), [&](auto i) {
          return has_role(roles(i), RoleTag::SpearInfantry);
        }));
    int const floor_share =
        std::max(1, static_cast<int>(std::lround(static_cast<float>(n) / 5.0F)));
    triarii = std::clamp(spears, floor_share, std::max(floor_share, n / 3));
    // An odd count in front lets the principes stand one fewer than the hastati,
    // every one of them on a gap, symmetric about the centre.
    if ((n - triarii) % 2 == 0) {
      if ((triarii + 1) * 2 <= n) {
        ++triarii;
      } else if (triarii > 1) {
        --triarii;
      }
    }
    int const front_two = n - triarii;
    principes = front_two / 2;
  }

  // Veterans (spearmen, then heavy infantry) go to the rear line; any remaining
  // spears stand with the principes, the youngest troops in front.
  auto seniority = [&](std::size_t i) {
    RoleTagSet const r = roles(i);
    if (has_role(r, RoleTag::SpearInfantry)) {
      return 0;
    }
    if (has_role(r, RoleTag::HeavyInfantry)) {
      return 1;
    }
    return 2;
  };
  Indices ordered = parts.infantry;
  std::stable_sort(ordered.begin(), ordered.end(), [&](std::size_t a, std::size_t b) {
    return seniority(a) < seniority(b);
  });
  Indices rear_line(ordered.begin(), ordered.begin() + triarii);
  Indices middle_line(ordered.begin() + triarii,
                      ordered.begin() + triarii + principes);
  Indices front_line(ordered.begin() + triarii + principes, ordered.end());
  sort_by_lateral(rear_line);
  sort_by_lateral(middle_line);
  sort_by_lateral(front_line);

  float const maniple_width = std::max(widest(parts.infantry), 1.0F);
  float const lane = std::max(maniple_width, k_min_maniple_lane) * m_gap_scale;
  float pitch = maniple_width + lane;
  auto const hastati = static_cast<int>(front_line.size());
  if (m_params.frontage > 0.01F && hastati > 1) {
    pitch = std::max(pitch, m_params.frontage / static_cast<float>(hastati - 1));
  }
  float const line_gap = std::max(0.75F * deepest(parts.infantry), 2.0F * m_gaps.rank);

  float const front_phase = hastati % 2 == 0 ? 0.5F : 0.0F;
  float const middle_phase = front_phase > 0.0F ? 0.0F : 0.5F;

  int const screen_rank = 0;
  int const first_line_rank = 1;
  float z = 0.0F;
  float previous_depth = 0.0F;
  int rank = first_line_rank;
  struct Line {
    const Indices* members;
    float phase;
    BattleBand band;
  };
  for (const Line& line : {Line{&front_line, front_phase, BattleBand::Hastati},
                           Line{&middle_line, middle_phase, BattleBand::Principes},
                           Line{&rear_line, front_phase, BattleBand::Triarii}}) {
    if (line.members->empty()) {
      continue;
    }
    float const depth = deepest(*line.members);
    if (rank > first_line_rank) {
      z -= previous_depth * 0.5F + line_gap + depth * 0.5F;
    }
    auto const xs =
        lattice_positions(static_cast<int>(line.members->size()), line.phase, pitch);
    for (std::size_t k = 0; k < line.members->size(); ++k) {
      put((*line.members)[k], xs[k], z, line.band, rank, static_cast<int>(k));
    }
    previous_depth = depth;
    ++rank;
  }
  float const army_rear = z - previous_depth * 0.5F;
  float const front_depth = deepest(front_line);
  float const front_half_span = span_of(front_line);

  Indices screen = parts.screen;
  screen.insert(screen.end(), parts.elephants.begin(), parts.elephants.end());
  sort_by_lateral(screen);
  if (!screen.empty()) {
    float const screen_z = front_depth * 0.5F + k_screen_rank_gaps * m_gaps.rank +
                           deepest(screen) * 0.5F;
    place_row(screen,
              screen_z,
              std::max(0.0F, 2.0F * front_half_span - maniple_width),
              BattleBand::Screen,
              screen_rank);
    for (auto const i : parts.elephants) {
      m_slots[i].band = BattleBand::Elephants;
    }
  }

  Indices left;
  Indices right;
  split_sides(parts.cavalry, left, right);
  float const lines_half_span =
      std::max({front_half_span, span_of(middle_line), span_of(rear_line)});
  float const wing_inner = lines_half_span + lane * 0.5F;
  place_wing(left, -1.0F, wing_inner, front_depth * 0.5F, BattleBand::None, 1);
  place_wing(right, 1.0F, wing_inner, front_depth * 0.5F, BattleBand::None, 1);

  place_rear_row(parts.other, army_rear, rank);
}

// Hannibal's lunate line: the centre bows toward the enemy and the heavy wings
// stand back level with its ends. Allied contingents (nations that are not
// playable, i.e. the Gauls and Iberians) take the centre when the infantry is
// mixed; otherwise the middle half of the infantry does.
void BattleOrderLayout::place_convex_crescent() {
  auto parts = partition();
  Indices centre;
  Indices wings;
  bool const mixed =
      std::any_of(parts.infantry.begin(),
                  parts.infantry.end(),
                  [&](auto i) { return allied(i); }) &&
      std::any_of(
          parts.infantry.begin(), parts.infantry.end(), [&](auto i) { return !allied(i); });
  if (mixed) {
    for (auto const i : parts.infantry) {
      (allied(i) ? centre : wings).push_back(i);
    }
  } else {
    auto const n = parts.infantry.size();
    std::size_t const per_side = n >= 3U ? std::max<std::size_t>(1U, n / 4U) : 0U;
    for (std::size_t k = 0; k < n; ++k) {
      bool const wing = k < per_side || k >= n - per_side;
      (wing ? wings : centre).push_back(parts.infantry[k]);
    }
  }
  sort_by_lateral(centre);
  sort_by_lateral(wings);

  float const centre_width = std::max(widest(centre), 1.0F);
  float const centre_depth = std::max(deepest(centre), 1.0F);
  float const pitch = centre_width + m_gaps.lateral;
  int const ranks = std::max(
      1,
      (static_cast<int>(centre.size()) + k_crescent_centre_per_rank - 1) /
          k_crescent_centre_per_rank);
  int const per_rank =
      (static_cast<int>(centre.size()) + ranks - 1) / std::max(1, ranks);
  float const half_centre =
      static_cast<float>(std::max(0, per_rank - 1)) * pitch * 0.5F +
      centre_width * 0.5F;
  float const bulge =
      std::max(k_crescent_bulge_share * half_centre, 2.0F * centre_depth);

  // Front rank first, so the outermost troops of every rank sit on the arc.
  std::size_t cursor = 0;
  for (int r = 0; r < ranks && cursor < centre.size(); ++r) {
    std::size_t const remaining = centre.size() - cursor;
    auto const rows_left = static_cast<std::size_t>(ranks - r);
    std::size_t const in_rank = (remaining + rows_left - 1U) / rows_left;
    auto const xs = lattice_positions(
        static_cast<int>(in_rank), in_rank % 2U == 0U ? 0.5F : 0.0F, pitch);
    float const row_z = -static_cast<float>(r) * (centre_depth + m_gaps.rank);
    for (std::size_t k = 0; k < in_rank; ++k) {
      auto const index = centre[cursor + k];
      float const t = std::clamp(xs[k] / half_centre, -1.0F, 1.0F);
      float const weight = 1.0F - t * t;
      float const slope = -2.0F * bulge * xs[k] / (half_centre * half_centre);
      float const facing = std::atan2(-slope, 1.0F) * k_rad_to_deg * k_crescent_turn_share;
      put(index,
          xs[k],
          row_z + bulge * weight,
          BattleBand::CrescentCentre,
          r,
          static_cast<int>(k),
          facing);
      m_slots[index].yield_depth = k_crescent_yield_reach * bulge * weight;
    }
    cursor += in_rank;
  }

  Indices left;
  Indices right;
  split_sides(wings, left, right);
  float const wing_inner = half_centre + 2.0F * m_gaps.lateral;
  float const front_z = centre_depth * 0.5F;
  place_wing(left, -1.0F, wing_inner, front_z, BattleBand::CrescentWing, 0);
  place_wing(right, 1.0F, wing_inner, front_z, BattleBand::CrescentWing, 0);

  Indices cav_left;
  Indices cav_right;
  split_sides(parts.cavalry, cav_left, cav_right);
  float const wings_reach =
      std::max({span_of(left), span_of(right), half_centre + m_gaps.lateral});
  float const cavalry_inner = wings_reach + 2.0F * m_gaps.lateral;
  place_wing(cav_left, -1.0F, cavalry_inner, front_z, BattleBand::None, 0);
  place_wing(cav_right, 1.0F, cavalry_inner, front_z, BattleBand::None, 0);

  float const tip = bulge + centre_depth * 0.5F;
  float screen_z = tip;
  if (!parts.elephants.empty()) {
    screen_z += k_elephant_screen_rank_gaps * m_gaps.rank + deepest(parts.elephants) * 0.5F;
    place_row(parts.elephants,
              screen_z,
              2.0F * half_centre * k_elephant_screen_span_share,
              BattleBand::Elephants,
              0);
    screen_z += deepest(parts.elephants) * 0.5F;
  }
  if (!parts.screen.empty()) {
    place_row(parts.screen,
              screen_z + k_screen_rank_gaps * m_gaps.rank + deepest(parts.screen) * 0.5F,
              2.0F * half_centre,
              BattleBand::Screen,
              0);
  }

  Indices placed = centre;
  placed.insert(placed.end(), wings.begin(), wings.end());
  placed.insert(placed.end(), parts.cavalry.begin(), parts.cavalry.end());
  place_rear_row(parts.other, placed.empty() ? 0.0F : rear_of(placed), ranks + 1);
}

// War elephants spaced in a row well ahead of the army, which stands behind
// them in its faction battle line.
void BattleOrderLayout::place_elephant_screen() {
  Indices elephants;
  Indices body;
  for (std::size_t i = 0; i < m_slots.size(); ++i) {
    (kind_of(roles(i)) == Kind::Elephant ? elephants : body).push_back(i);
  }
  sort_by_lateral(elephants);

  float main_front = 0.0F;
  float main_half_span = 0.0F;
  if (!body.empty()) {
    std::vector<FormationSlot> sub_slots;
    SlotExtents sub_extents;
    for (auto const i : body) {
      sub_slots.push_back(m_slots[i]);
      sub_extents.half_width.push_back(hw(i));
      sub_extents.half_depth.push_back(hd(i));
      sub_extents.roles.push_back(roles(i));
      sub_extents.troop.push_back(troop(i));
      sub_extents.allied.push_back(allied(i) ? 1U : 0U);
    }
    SilhouetteParams const sub_params{ArmyFormationIntent::FactionDefault,
                                      0.0F,
                                      m_params.spacing,
                                      m_params.options,
                                      m_params.tmpl};
    regularize_silhouette(sub_slots, sub_extents, sub_params);
    for (std::size_t k = 0; k < body.size(); ++k) {
      auto& slot = m_slots[body[k]];
      slot.local_offset = sub_slots[k].local_offset;
      slot.local_facing = sub_slots[k].local_facing;
      slot.rank = sub_slots[k].rank + 1;
      slot.file = sub_slots[k].file;
      slot.band = BattleBand::None;
      slot.yield_depth = 0.0F;
      slot.manoeuvre_offset = QVector3D();
      slot.manoeuvre_facing = 0.0F;
    }
    main_front = front_of(body);
    main_half_span = span_of(body);
  }
  if (elephants.empty()) {
    return;
  }

  float const elephant_width = widest(elephants);
  float const min_pitch =
      elephant_width + std::max(k_elephant_min_gap, 1.5F * elephant_width) * m_gap_scale;
  float const span = 2.0F * main_half_span * k_elephant_screen_span_share;
  float const pitch =
      elephants.size() > 1U
          ? std::max(min_pitch, span / static_cast<float>(elephants.size() - 1U))
          : 0.0F;
  auto const xs = lattice_positions(static_cast<int>(elephants.size()),
                                    elephants.size() % 2U == 0U ? 0.5F : 0.0F,
                                    std::max(pitch, 0.01F));
  float const z = main_front + k_elephant_screen_rank_gaps * m_gaps.rank +
                  deepest(elephants) * 0.5F;
  for (std::size_t k = 0; k < elephants.size(); ++k) {
    put(elephants[k], xs[k], z, BattleBand::Elephants, 0, static_cast<int>(k));
  }
}

} // namespace

auto lattice_positions(int count, float phase, float pitch) -> std::vector<float> {
  std::vector<float> xs;
  if (count <= 0) {
    return xs;
  }
  xs.reserve(static_cast<std::size_t>(count));
  auto const start = static_cast<float>(
      std::lround(-static_cast<float>(count - 1) * 0.5F - phase));
  for (int i = 0; i < count; ++i) {
    xs.push_back((start + static_cast<float>(i) + phase) * pitch);
  }
  return xs;
}

auto place_battle_order(std::vector<FormationSlot>& slot_list,
                        const SlotExtents& extents,
                        const SilhouetteParams& params,
                        const BattleOrderGaps& gaps) -> bool {
  if (!is_battle_order_intent(params.intent)) {
    return false;
  }
  return BattleOrderLayout(slot_list, extents, params, gaps).run();
}

} // namespace Game::Formation::planning
