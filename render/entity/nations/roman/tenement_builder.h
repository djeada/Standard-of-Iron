#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/roman/tenement_authoring.h"
#include "render/entity/tenement_layout.h"
#include "render/entity/tenement_shell.h"

// The builder behind build_tenement_desc, shared by the files that author the
// insula's shell, its street and court dressing, and its roof.
namespace Render::GL::Roman::TenementDetail {

using namespace TenementShellDims;

inline constexpr float k_pi = 3.14159265F;
inline constexpr float k_eave_overhang = 0.07F;
// Deep enough that the shop reads as a room behind the counter, not a niche.
inline constexpr float k_shop_depth = 0.80F;

inline constexpr QVector3D k_void{0.115F, 0.095F, 0.080F};
inline constexpr QVector3D k_interior_plaster{0.50F, 0.41F, 0.32F};
inline constexpr QVector3D k_interior_floor{0.40F, 0.35F, 0.30F};
inline constexpr QVector3D k_signinum{0.66F, 0.47F, 0.38F};
inline constexpr QVector3D k_iron{0.20F, 0.20F, 0.21F};
inline constexpr QVector3D k_leaf{0.27F, 0.42F, 0.17F};
inline constexpr QVector3D k_leaf_light{0.36F, 0.50F, 0.21F};
inline constexpr QVector3D k_linen{0.86F, 0.80F, 0.66F};
inline constexpr QVector3D k_charred{0.15F, 0.12F, 0.10F};
inline constexpr QVector3D k_paving{0.63F, 0.60F, 0.54F};
inline constexpr QVector3D k_paving_edge{0.50F, 0.48F, 0.43F};

using Opening = TenementOpening;
using Side = TenementSide;

class TenementBuilder : public TenementShell {
public:
  TenementBuilder(BuildingArchetypeDesc& desc, BuildingState state, int variant);
  void build();

private:
  [[nodiscard]] auto front_openings(int storey) const -> std::vector<Opening>;
  [[nodiscard]] auto rear_openings(int storey) const -> std::vector<Opening>;
  void add_pavement();
  void add_core();
  void add_shell();
  // The carriage passage: a round-headed opening, cut as stepped strips that
  // the voussoirs then cover.
  void add_passage_front();
  void add_jar(const QVector3D& foot, float scale, const QVector3D& clay);
  void add_pot(const QVector3D& foot, int salt);
  void add_window_dressing(Side side, const Opening& opening, int salt, bool shutters);
  void add_door_leaf(Side side, const Opening& opening, const QVector3D& timber);
  void add_shop_front();
  void add_door_front();
  void add_passage_dressing();
  void add_ground_storey_dressing();
  void add_balcony(int storey);
  // The cheap way to add a storey: a timber frame with plastered panels
  // (opus craticium) on top of the masonry house.
  [[nodiscard]] auto timber_framed(int storey) const -> bool;
  void add_timber_frame(int storey);
  void add_upper_storey_dressing();
  void add_rear_dressing();
  // The party wall carried up into the gable: coarse courses fill the
  // triangle, and a raking slab under each roof slope hides their steps.
  void add_gable_end(float sign, float rise);
  void add_chimney(const QVector3D& vent);
  // Tegulae laid in overlapping courses with a cover tile over every joint and
  // an antefix closing each cover tile at the eave.
  void add_pantiles(float x0, float x1, float rise);
  void add_gable_roof();
  void add_terrace_roof();
  void add_roof();
  void add_ruin();
};

} // namespace Render::GL::Roman::TenementDetail
