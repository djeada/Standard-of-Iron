#include "render/entity/nations/roman/tenement_authoring.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/nation_motifs.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/nations/roman/tenement_builder.h"

namespace Render::GL::Roman::TenementDetail {

using namespace TenementLot;

TenementBuilder::TenementBuilder(BuildingArchetypeDesc& desc,
                                 BuildingState state,
                                 int variant)
    : TenementShell(desc, state, variant, k_tenement_wall_slot, k_tenement_dado_slot) {
}

void TenementBuilder::build() {
  add_pavement();
  if (m_state == BuildingState::Destroyed) {
    add_shell();
    add_ruin();
    return;
  }
  add_shell();
  add_ground_storey_dressing();
  add_upper_storey_dressing();
  add_rear_dressing();
  add_roof();
  add_ruin();
}

auto TenementBuilder::front_openings(int storey) const -> std::vector<Opening> {
  const float p = k_pavement_top;
  if (storey == 0) {
    switch (m_traits.ground) {
    case TenementGround::Door:
      return {{-0.86F, -0.58F, p + 0.56F, p + 0.82F},
              {-0.24F, 0.24F, p, p + 0.86F},
              {0.58F, 0.86F, p + 0.56F, p + 0.82F}};
    case TenementGround::Passage:
      return {{-0.92F, -0.64F, p + 0.56F, p + 0.82F},
              {-0.34F, 0.34F, p, p + 0.90F},
              {0.56F, 0.96F, p + 0.34F, p + 0.76F}};
    case TenementGround::Shop:
    default:
      return {{-0.98F, 0.26F, p, p + 0.78F}, {0.52F, 0.94F, p, p + 0.80F}};
    }
  }
  const float fy = floor_y(storey);
  const bool attic = storey == m_traits.storeys - 1 && m_traits.storeys >= 3;
  const float sill = fy + (attic ? 0.30F : 0.22F);
  const float head = fy + (attic ? 0.56F : 0.58F);
  const bool balcony = storey == m_traits.balcony_storey;
  if (!balcony && ((m_variant + storey) % 3) == 1) {
    return {{-0.63F, -0.33F, sill, head}, {0.33F, 0.63F, sill, head}};
  }
  std::vector<Opening> out{{-0.85F, -0.59F, sill, head}, {0.59F, 0.85F, sill, head}};
  if (balcony) {
    out.push_back({-0.15F, 0.15F, fy + 0.012F, fy + 0.60F});
  } else {
    out.push_back({-0.13F, 0.13F, sill, head});
  }
  return out;
}

auto TenementBuilder::rear_openings(int storey) const -> std::vector<Opening> {
  const float p = k_pavement_top;
  if (storey == 0) {
    if (m_traits.ground == TenementGround::Passage) {
      return {{-0.94F, -0.66F, p + 0.50F, p + 0.78F},
              {-0.34F, 0.34F, 0.0F, p + 0.86F},
              {0.52F, 0.86F, 0.02F, 0.80F}};
    }
    return {{-0.84F, -0.50F, 0.02F, 0.80F}, {0.30F, 0.58F, p + 0.50F, p + 0.78F}};
  }
  const float fy = floor_y(storey);
  const float shift = ((m_variant + storey) % 2) == 0 ? 0.0F : 0.12F;
  return {{-0.70F + shift, -0.46F + shift, fy + 0.26F, fy + 0.56F},
          {0.38F + shift, 0.62F + shift, fy + 0.26F, fy + 0.56F}};
}

void TenementBuilder::add_pavement() {
  BuildingPartLabel const label(m_desc, "pavement");
  box(-k_half_width,
      k_half_width,
      0.0F,
      k_pavement_top,
      k_front,
      k_pavement_reach - 0.04F,
      k_paving);
  box(-k_half_width,
      k_half_width,
      0.0F,
      k_pavement_top + 0.008F,
      k_pavement_reach - 0.04F,
      k_pavement_reach,
      k_paving_edge);
}

void TenementBuilder::add_core() {
  BuildingPartLabel const label(m_desc, "core");
  const float inner_rear = k_rear + k_wall;
  const float inner_front = k_front - k_wall;
  const float ground_top = ceiling_y(0);
  if (m_state == BuildingState::Destroyed) {
    box(-k_inner_half,
        k_inner_half,
        0.0F,
        k_pavement_top + 0.14F,
        inner_rear,
        inner_front,
        BuildingPalette::k_limestone_dark * 0.82F);
    return;
  }

  switch (m_traits.ground) {
  case TenementGround::Shop: {
    const float shop_back = k_front - k_shop_depth;
    box(-k_inner_half, k_inner_half, 0.0F, ground_top, inner_rear, shop_back, k_void);
    box(0.39F, k_inner_half, 0.0F, ground_top, shop_back, inner_front, k_void);
    box(-k_inner_half,
        0.39F,
        0.0F,
        k_pavement_top + 0.006F,
        shop_back,
        inner_front,
        k_interior_floor);
    box(-k_inner_half,
        0.375F,
        k_pavement_top + 0.006F,
        ground_top,
        shop_back,
        shop_back + 0.012F,
        k_interior_plaster);
    box(0.375F,
        0.39F,
        k_pavement_top + 0.006F,
        ground_top,
        shop_back + 0.012F,
        inner_front,
        k_interior_plaster * 0.92F);
    break;
  }
  case TenementGround::Passage:
    box(-k_inner_half, -0.34F, 0.0F, ground_top, inner_rear, inner_front, k_void);
    box(0.34F, k_inner_half, 0.0F, ground_top, inner_rear, inner_front, k_void);
    box(-0.34F,
        0.34F,
        0.0F,
        k_pavement_top * 0.6F,
        k_rear,
        k_front,
        k_paving_edge * 0.92F);
    break;
  case TenementGround::Door:
  default:
    box(-k_inner_half, k_inner_half, 0.0F, ground_top, inner_rear, inner_front, k_void);
    break;
  }

  if (m_traits.storeys < 2) {
    return;
  }
  if (intact()) {
    box(-k_inner_half,
        k_inner_half,
        ground_top,
        m_top,
        inner_rear,
        inner_front,
        k_void);
    return;
  }
  const float broken_floor = floor_y(m_traits.storeys - 1);
  box(-k_inner_half,
      k_inner_half,
      ground_top,
      broken_floor,
      inner_rear,
      inner_front,
      k_void);
  box(-k_inner_half, k_break_x, broken_floor, m_top, inner_rear, inner_front, k_void);
  box(k_break_x,
      k_inner_half,
      broken_floor,
      broken_floor + 0.10F,
      inner_rear,
      inner_front,
      k_charred * 1.5F);
}

void TenementBuilder::add_shell() {
  add_core();
  {
    BuildingPartLabel const label(m_desc, "party_wall");
    add_side_wall(-1.0F);
    add_side_wall(1.0F);
  }
  for (int storey = 0; storey < storeys(); ++storey) {
    {
      BuildingPartLabel const label(m_desc, "street_wall");
      if (storey == 0 && m_traits.ground == TenementGround::Passage) {
        add_passage_front();
      } else {
        wall_storey(Side::Front, storey, front_openings(storey));
      }
    }
    BuildingPartLabel const label(m_desc, "court_wall");
    wall_storey(Side::Rear, storey, rear_openings(storey));
  }
}

void TenementBuilder::add_passage_front() {
  auto openings = front_openings(0);
  wall_storey(Side::Front, 0, openings);
  const float spring = k_pavement_top + 0.56F;
  const float crown = k_pavement_top + 0.90F;
  constexpr float k_radius = 0.34F;
  constexpr int k_strips = 8;
  const float step = (2.0F * k_radius) / static_cast<float>(k_strips);
  for (int index = 0; index < k_strips; ++index) {
    const float x0 = -k_radius + (step * static_cast<float>(index));
    const float mid = x0 + (step * 0.5F);
    const float arch =
        spring + std::sqrt(std::max(0.0F, (k_radius * k_radius) - (mid * mid)));
    wall_piece(Side::Front, x0, x0 + step, arch, crown, false);
  }
  if (m_state == BuildingState::Destroyed) {
    return;
  }
  BuildingPartLabel const label(m_desc, "passage_arch");
  constexpr int k_voussoirs = 9;
  for (int index = 0; index < k_voussoirs; ++index) {
    const float degrees =
        180.0F * (static_cast<float>(index) + 0.5F) / static_cast<float>(k_voussoirs);
    const float radians = degrees * k_pi / 180.0F;
    const float reach = k_radius + 0.030F;
    const float x = std::cos(radians) * reach;
    if (x > k_break_x && !intact() && cap(x) < crown) {
      continue;
    }
    m_desc.add_rotated_box(
        QVector3D(x, spring + (std::sin(radians) * reach), k_front - 0.028F),
        QVector3D(0.066F, 0.036F, 0.046F),
        QVector3D(0.0F, 0.0F, degrees - 90.0F),
        weathered(index == k_voussoirs / 2 ? BuildingPalette::k_marble
                                           : BuildingPalette::k_limestone,
                  index + 5,
                  0.05F));
  }
  for (const float side : {-1.0F, 1.0F}) {
    trim(Side::Front,
         side > 0.0F ? k_radius - 0.01F : -(k_radius + 0.085F),
         side > 0.0F ? k_radius + 0.085F : -(k_radius - 0.01F),
         spring - 0.045F,
         spring,
         0.024F,
         0.06F,
         BuildingPalette::k_limestone_shade);
  }
}

void TenementBuilder::add_ruin() {
  add_ruin_dressing(m_desc,
                    RuinDressing{.center = QVector3D(0.0F, 0.0F, k_mid_z),
                                 .extent = QVector3D(1.02F, 0.0F, 1.02F),
                                 .stone = BuildingPalette::k_limestone_shade,
                                 .stone_dark = BuildingPalette::k_limestone_dark,
                                 .timber = BuildingPalette::k_terracotta_dark * 0.45F,
                                 .ground_y = k_pavement_top + 0.14F,
                                 .apron_y = 0.02F,
                                 .scale = 1.15F,
                                 .seed = 211 + (m_variant * 13)});
}

} // namespace Render::GL::Roman::TenementDetail

namespace Render::GL::Roman {

using namespace TenementLot;
using namespace TenementDetail;

auto tenement_vent(int variant) -> QVector3D {
  const TenementTraits& traits = tenement_traits(variant);
  const float top = tenement_wall_top(traits);
  if (traits.roof == TenementRoof::Terrace) {
    return {-0.72F, top + 0.62F, k_rear + 0.30F};
  }
  return {
      0.52F, top + (traits.roof_rise * 0.5F) + 0.30F, k_mid_z - (k_half_depth * 0.5F)};
}

auto build_tenement_desc(BuildingState state, int variant) -> BuildingArchetypeDesc {
  BuildingArchetypeDesc desc("roman_tenement_variant_" + std::to_string(variant));
  TenementBuilder(desc, state, variant).build();

  {
    BuildingPartMaterial const stone(desc, k_building_material_stone);
    const float foot = TenementLot::k_pavement_top;
    for (const float face :
         {TenementLot::k_front + 0.004F, TenementLot::k_rear - 0.004F}) {
      desc.add_box(QVector3D(0.0F, foot + 0.065F, face),
                   QVector3D(TenementLot::k_half_width, 0.065F, 0.004F),
                   Motif::Rome::k_tufa_dark);
    }
  }

  const Motif::Setter set(desc, state);
  if (tenement_traits(variant).roof == TenementRoof::Gable) {
    const QVector3D ridge =
        set.highest(-0.2F, 0.2F, TenementLot::k_rear, TenementLot::k_front);
    for (const float side : {-1.0F, 1.0F}) {
      Motif::add_roman_acroterion(
          desc,
          set.at(side * (TenementLot::k_half_width - 0.09F), ridge.z()),
          1.5F,
          true);
    }
    if (variant % 3 == 1) {
      Motif::add_roman_raven(desc, set.at(0.35F, ridge.z()), 25.0F, 1.4F);
    }
  }
  return desc;
}

} // namespace Render::GL::Roman
