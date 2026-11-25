#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/tenement_layout.h"

namespace Render::GL {

namespace TenementShellDims {
inline constexpr float k_inner_half = TenementLot::k_half_width - TenementLot::k_wall;
inline constexpr float k_mid_z = (TenementLot::k_front + TenementLot::k_rear) * 0.5F;
inline constexpr float k_half_depth =
    (TenementLot::k_front - TenementLot::k_rear) * 0.5F;

inline constexpr float k_dado_top = TenementLot::k_pavement_top + 0.50F;

inline constexpr float k_break_x = -0.22F;
} // namespace TenementShellDims

struct TenementOpening {
  float x0;
  float x1;
  float y0;
  float y1;
};

enum class TenementSide {
  Front,
  Rear
};

class TenementShell {
protected:
  TenementShell(BuildingArchetypeDesc& desc,
                BuildingState state,
                int variant,
                std::uint8_t wall_slot,
                std::uint8_t dado_slot);

  [[nodiscard]] auto intact() const -> bool;
  [[nodiscard]] auto storeys() const -> int;
  [[nodiscard]] auto floor_y(int storey) const -> float;
  [[nodiscard]] auto ceiling_y(int storey) const -> float;
  [[nodiscard]] auto roll(int salt) const -> float;

  [[nodiscard]] auto cap(float x) const -> float;
  void box(float x0,
           float x1,
           float y0,
           float y1,
           float z0,
           float z1,
           const QVector3D& color);
  void slot_box(
      float x0, float x1, float y0, float y1, float z0, float z1, std::uint8_t slot);
  [[nodiscard]] static auto face(TenementSide side) -> float;
  [[nodiscard]] static auto outward(TenementSide side) -> float;

  void trim(TenementSide side,
            float x0,
            float x1,
            float y0,
            float y1,
            float proud,
            float depth,
            const QVector3D& color);

  void wall_piece(TenementSide side, float x0, float x1, float y0, float y1, bool dado);
  void
  wall_storey(TenementSide side, int storey, std::vector<TenementOpening> openings);
  void add_side_wall(float sign);

  BuildingArchetypeDesc& m_desc;
  BuildingState m_state;
  int m_variant;
  TenementTraits m_traits;
  float m_top;
  std::uint8_t m_wall_slot;
  std::uint8_t m_dado_slot;
};

} // namespace Render::GL
