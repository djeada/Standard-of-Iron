#pragma once

#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Game::Formation {

using UnitLayoutId = std::uint16_t;
inline constexpr UnitLayoutId k_invalid_layout = 0xFFFFU;

enum class UnitLayoutShape : std::uint8_t {
  Ranks,
  Wedge,
  LooseOrder,
  Column,
  Cluster,
  Circle,
  Shell,
  Arc
};

enum class UnitLayoutState : std::uint8_t {
  Normal,
  Defensive,
  Attacking,
  Braced,
  Marching,
  Routing,
  Disrupted
};

[[nodiscard]] auto layout_shape_to_string(UnitLayoutShape shape) -> const char*;
[[nodiscard]] auto
try_parse_layout_shape(const QString& value) -> std::optional<UnitLayoutShape>;

[[nodiscard]] auto layout_state_to_string(UnitLayoutState state) -> const char*;
[[nodiscard]] auto
try_parse_layout_state(const QString& value) -> std::optional<UnitLayoutState>;

struct UnitLayoutStyle {
  std::string id;
  UnitLayoutShape shape{UnitLayoutShape::Ranks};

  float lateral_spacing_scale{1.0F};
  float depth_spacing_scale{1.0F};

  float rank_stagger{0.0F};
  float rank_echelon{0.0F};
  float rank_arc{0.0F};

  float front_rank_tightening{0.0F};
  float rear_rank_loosening{0.0F};
  float rear_depth_bias{0.0F};

  float lateral_jitter{0.0F};
  float depth_jitter{0.0F};
  float rear_jitter_gain{0.0F};
  float facing_jitter_degrees{0.0F};

  float wedge_slope{0.0F};
  float wedge_growth{2.0F};
  float cluster_pull{0.0F};
  float cluster_size{4.0F};
  float radius_scale{2.8F};

  float file_grouping{0.0F};
  float group_gap{0.0F};
  float group_depth_stagger{0.0F};

  float weapon_clearance{0.0F};
  float min_separation_scale{0.55F};

  float column_files{2.0F};

  [[nodiscard]] auto lateral_step(float spacing) const -> float {
    return spacing * lateral_spacing_scale;
  }
  [[nodiscard]] auto depth_step(float spacing) const -> float {
    return spacing * depth_spacing_scale * (1.0F + weapon_clearance);
  }
};

struct SoldierOffset {
  float offset_x{0.0F};
  float offset_z{0.0F};
  float yaw_offset{0.0F};
};

struct UnitLayoutQuery {
  UnitLayoutId layout{k_invalid_layout};
  int index{0};
  int row{0};
  int col{0};
  int rows{1};
  int cols{1};
  int count{0};
  float spacing{1.0F};
  std::uint32_t seed{0U};
  float formed_ratio{1.0F};
};

struct RankSlot {
  int row{0};
  int col{0};
  int rows{1};
  int cols{1};
  int rank_cols{1};
};

[[nodiscard]] auto rank_slot_for(int index, int count, int max_per_row) -> RankSlot;

class UnitLayoutLibrary {
public:
  static auto instance() -> UnitLayoutLibrary&;

  void reset_to_defaults();
  void clear();

  auto register_style(UnitLayoutStyle style) -> UnitLayoutId;

  [[nodiscard]] auto find(std::string_view name) const -> UnitLayoutId;

  [[nodiscard]] auto resolve(std::string_view doctrine,
                             std::string_view generic_name) const -> UnitLayoutId;

  [[nodiscard]] auto style(UnitLayoutId id) const -> const UnitLayoutStyle&;
  [[nodiscard]] auto name(UnitLayoutId id) const -> const std::string&;
  [[nodiscard]] auto size() const -> std::size_t { return m_styles.size(); }
  [[nodiscard]] auto names() const -> std::vector<std::string>;
  [[nodiscard]] auto contains(std::string_view name) const -> bool {
    return find(name) != k_invalid_layout;
  }

private:
  UnitLayoutLibrary();

  std::vector<UnitLayoutStyle> m_styles;
  std::unordered_map<std::string, UnitLayoutId> m_by_name;
  UnitLayoutStyle m_fallback;
};

class UnitLayoutSystem {
public:
  static auto instance() -> UnitLayoutSystem&;

  [[nodiscard]] auto offset(const UnitLayoutQuery& query) const -> SoldierOffset;

  [[nodiscard]] auto
  compute(UnitLayoutId layout,
          int count,
          int max_per_row,
          float spacing,
          std::uint32_t seed,
          float formed_ratio = 1.0F) const -> std::vector<SoldierOffset>;

  [[nodiscard]] static auto rows_for(int count, int max_per_row) -> int;
};

[[nodiscard]] auto apply_state_modifier(const UnitLayoutStyle& style,
                                        UnitLayoutState state) -> UnitLayoutStyle;

} // namespace Game::Formation
