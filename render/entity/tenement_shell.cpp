#include "render/entity/tenement_shell.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "render/entity/building_decay.h"

namespace Render::GL {

using namespace TenementLot;
using namespace TenementShellDims;

TenementShell::TenementShell(BuildingArchetypeDesc& desc,
                             BuildingState state,
                             int variant,
                             std::uint8_t wall_slot,
                             std::uint8_t dado_slot)
    : m_desc(desc)
    , m_state(state)
    , m_variant(variant)
    , m_traits(tenement_traits(variant))
    , m_top(tenement_wall_top(m_traits))
    , m_wall_slot(wall_slot)
    , m_dado_slot(dado_slot) {
}

auto TenementShell::intact() const -> bool {
  return m_state == BuildingState::Normal;
}

auto TenementShell::storeys() const -> int {
  return m_state == BuildingState::Destroyed ? 1 : m_traits.storeys;
}

auto TenementShell::floor_y(int storey) const -> float {
  if (storey <= 0) {
    return k_pavement_top;
  }
  return k_pavement_top + k_ground_storey +
         (static_cast<float>(storey - 1) * k_upper_storey);
}

auto TenementShell::ceiling_y(int storey) const -> float {
  return floor_y(storey + 1);
}

auto TenementShell::roll(int salt) const -> float {
  return decay_hash((m_variant * 977) + (salt * 131) + 19);
}

auto TenementShell::cap(float x) const -> float {
  const int bucket = static_cast<int>(std::floor((x + 2.0F) / 0.30F));
  switch (m_state) {
  case BuildingState::Damaged:
    if (x < k_break_x) {
      return std::numeric_limits<float>::max();
    }
    return floor_y(m_traits.storeys - 1) +
           ((m_traits.storeys > 1 ? k_upper_storey : k_ground_storey) *
            (0.26F + (0.52F * roll(bucket + 40))));
  case BuildingState::Destroyed:
    return k_pavement_top + (k_ground_storey * (0.26F + (0.46F * roll(bucket + 70))));
  case BuildingState::Normal:
  default:
    return std::numeric_limits<float>::max();
  }
}

void TenementShell::box(float x0,
                        float x1,
                        float y0,
                        float y1,
                        float z0,
                        float z1,
                        const QVector3D& color) {
  if (x1 - x0 < 1.0e-4F || y1 - y0 < 1.0e-4F || z1 - z0 < 1.0e-4F) {
    return;
  }
  m_desc.add_box(QVector3D((x0 + x1) * 0.5F, (y0 + y1) * 0.5F, (z0 + z1) * 0.5F),
                 QVector3D((x1 - x0) * 0.5F, (y1 - y0) * 0.5F, (z1 - z0) * 0.5F),
                 color);
}

void TenementShell::slot_box(
    float x0, float x1, float y0, float y1, float z0, float z1, std::uint8_t slot) {
  if (x1 - x0 < 1.0e-4F || y1 - y0 < 1.0e-4F || z1 - z0 < 1.0e-4F) {
    return;
  }

  const bool plaster = slot == m_wall_slot || slot == m_dado_slot;
  BuildingPartMaterial const material(
      m_desc, plaster ? k_building_material_stone : m_desc.material());
  m_desc.add_palette_box(
      QVector3D((x0 + x1) * 0.5F, (y0 + y1) * 0.5F, (z0 + z1) * 0.5F),
      QVector3D((x1 - x0) * 0.5F, (y1 - y0) * 0.5F, (z1 - z0) * 0.5F),
      slot);
}

auto TenementShell::face(TenementSide side) -> float {
  return side == TenementSide::Front ? k_front : k_rear;
}

auto TenementShell::outward(TenementSide side) -> float {
  return side == TenementSide::Front ? 1.0F : -1.0F;
}

void TenementShell::trim(TenementSide side,
                         float x0,
                         float x1,
                         float y0,
                         float y1,
                         float proud,
                         float depth,
                         const QVector3D& color) {
  const float outer = face(side) + (outward(side) * proud);
  const float inner = outer - (outward(side) * depth);
  box(x0, x1, y0, y1, std::min(outer, inner), std::max(outer, inner), color);
}

void TenementShell::wall_piece(
    TenementSide side, float x0, float x1, float y0, float y1, bool dado) {
  const float z_outer = face(side);
  const float z_inner = z_outer - (outward(side) * k_wall);
  const float z0 = std::min(z_outer, z_inner);
  const float z1 = std::max(z_outer, z_inner);
  const auto lay_strip = [&](float from, float to) {
    const float top = std::min(y1, cap((from + to) * 0.5F));
    if (top <= y0) {
      return;
    }
    if (dado && y0 < k_dado_top) {
      slot_box(from, to, y0, std::min(top, k_dado_top), z0, z1, m_dado_slot);
      slot_box(from, to, k_dado_top, top, z0, z1, m_wall_slot);
    } else {
      slot_box(from, to, y0, top, z0, z1, m_wall_slot);
    }
  };
  if (intact() || (x1 - x0) <= 0.34F) {
    lay_strip(x0, x1);
    return;
  }
  const int strips = std::max(1, static_cast<int>(std::ceil((x1 - x0) / 0.30F)));
  const float step = (x1 - x0) / static_cast<float>(strips);
  for (int index = 0; index < strips; ++index) {
    lay_strip(x0 + (step * static_cast<float>(index)),
              x0 + (step * static_cast<float>(index + 1)));
  }
}

void TenementShell::wall_storey(TenementSide side,
                                int storey,
                                std::vector<TenementOpening> openings) {
  std::sort(
      openings.begin(),
      openings.end(),
      [](const TenementOpening& a, const TenementOpening& b) { return a.x0 < b.x0; });
  const float y0 = storey == 0 ? 0.0F : floor_y(storey);
  const float y1 = ceiling_y(storey);
  const bool dado = storey == 0 && side == TenementSide::Front;
  float cursor = -k_inner_half;
  for (const TenementOpening& opening : openings) {
    wall_piece(side, cursor, opening.x0, y0, y1, dado);
    wall_piece(side, opening.x0, opening.x1, y0, opening.y0, dado);
    wall_piece(side, opening.x0, opening.x1, opening.y1, y1, dado);
    cursor = opening.x1;
  }
  wall_piece(side, cursor, k_inner_half, y0, y1, dado);
}

void TenementShell::add_side_wall(float sign) {
  const float x0 = sign > 0.0F ? k_inner_half : -k_half_width;
  const float x1 = sign > 0.0F ? k_half_width : -k_inner_half;
  if (intact()) {
    slot_box(x0, x1, 0.0F, m_top, k_rear, k_front, m_wall_slot);
    return;
  }
  const bool destroyed = m_state == BuildingState::Destroyed;
  const float base = destroyed ? 0.0F : floor_y(m_traits.storeys - 1);
  if (!destroyed) {
    slot_box(x0, x1, 0.0F, base, k_rear, k_front, m_wall_slot);
    if (sign < 0.0F) {
      slot_box(x0, x1, base, m_top, k_rear, k_front, m_wall_slot);
      return;
    }
  }
  constexpr int k_strips = 5;
  const float step = (k_front - k_rear) / static_cast<float>(k_strips);
  const float storey_height = destroyed ? k_ground_storey : k_upper_storey;
  for (int index = 0; index < k_strips; ++index) {
    const float rise =
        storey_height * (0.24F + (0.56F * roll(index + (sign > 0.0F ? 110 : 130))));
    slot_box(x0,
             x1,
             base,
             base + rise,
             k_rear + (step * static_cast<float>(index)),
             k_rear + (step * static_cast<float>(index + 1)),
             m_wall_slot);
  }
}

} // namespace Render::GL
