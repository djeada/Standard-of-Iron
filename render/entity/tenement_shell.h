#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/tenement_layout.h"

// The part of a tenement every nation builds the same way: the lot, the party
// walls, facade slabs cut round their openings, and what damage leaves
// standing. A nation's builder derives from it and adds its own street front,
// windows and roof.
namespace Render::GL {

namespace TenementShellDims {
inline constexpr float k_inner_half = TenementLot::k_half_width - TenementLot::k_wall;
inline constexpr float k_mid_z = (TenementLot::k_front + TenementLot::k_rear) * 0.5F;
inline constexpr float k_half_depth =
    (TenementLot::k_front - TenementLot::k_rear) * 0.5F;
// A painted band along the street, from the pavement up to here.
inline constexpr float k_dado_top = TenementLot::k_pavement_top + 0.50F;
// Damage opens the house from this lateral position rightwards.
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
  // Walls take `wall_slot` from the instance palette; the street storey takes
  // `dado_slot` up to the dado line (pass the wall slot for no dado).
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
  // How high the wall still stands at a lateral position.
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
  // A box standing proud of a facade by `proud`, `depth` thick.
  void trim(TenementSide side,
            float x0,
            float x1,
            float y0,
            float y1,
            float proud,
            float depth,
            const QVector3D& color);
  // One piece of the facade slab, cut down to what damage leaves standing and
  // split at the painted dado on the street storey.
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
