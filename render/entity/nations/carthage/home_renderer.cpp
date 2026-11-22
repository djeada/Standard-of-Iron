#include "render/entity/nations/carthage/home_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "game/core/component.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_facade_depth.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/home_activity.h"
#include "render/entity/home_renderer_common.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/nations/carthage/home_authoring.h"
#include "render/entity/registry.h"
#include "render/frame/submitter.h"
#include "render/math/math_utils.h"

namespace Render::GL::Carthage {

using namespace home_detail;

namespace {

constexpr std::array<QVector3D, 5> k_roof_finishes{
    BuildingPalette::k_tile_red,
    QVector3D(0.80F, 0.76F, 0.66F),
    QVector3D(0.70F, 0.52F, 0.33F),
    QVector3D(0.47F, 0.29F, 0.22F),
    QVector3D(0.73F, 0.52F, 0.43F),
};

auto home_palette_slots(const QVector3D& team, std::uint32_t seed, BuildingState state)
    -> std::array<QVector3D, k_home_palette_slots> {
  const auto palette = make_palette(team);
  QVector3D roof = k_roof_finishes[home_tint_pick(seed, k_roof_finishes.size())] *
                   (1.0F + home_tint_jitter(seed) * 0.08F);
  roof = decayed_color(clamp_vec_01(roof), state, static_cast<int>(seed & 0xFFFFU));
  return {palette.team, roof};
}

auto home_archetype(BuildingState state, int variant) -> const RenderArchetype& {
  static const std::array<const BuildingArchetypeSet*, k_home_visual_variant_count>
      k_sets{&building_archetype_set("carthage_home"),
             &building_archetype_set("carthage_home_variant_1"),
             &building_archetype_set("carthage_home_variant_2"),
             &building_archetype_set("carthage_home_variant_3"),
             &building_archetype_set("carthage_home_variant_4"),
             &building_archetype_set("carthage_home_variant_5")};
  return k_sets[static_cast<std::size_t>(variant)]->for_state(state);
}

const std::array<TorchMount, 2> k_torches{{
    TorchMount{.at = QVector3D(-0.5F, 0.62F, 1.02F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(0.5F, 0.62F, 1.02F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
}};

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry& registry) {

  register_home_smoke_anchor(
      true,
      HomeSmokeAnchor{.vent = QVector3D(0.48F, 1.70F, -0.46F),
                      .smoke_tint = QVector3D(0.54F, 0.52F, 0.49F),
                      .plume_radius = 0.60F,
                      .doorstep = QVector3D(0.0F, 0.16F, 1.22F),
                      .outward = QVector3D(0.0F, 0.0F, 1.0F),

                      .cloth_top = QVector3D(-0.35F, 1.72F, 0.34F),
                      .cloth_span = 0.74F,
                      .cloth_indigo = true,
                      .cloth_at_door = false,
                      .lamp = QVector3D(0.0F, 0.165F, 1.20F),

                      .window = QVector3D(1.045F, 0.58F, 0.32F),
                      .shutter_width = 0.07F,
                      .shutter_height = 0.44F,

                      .line_a = QVector3D(0.0F, 1.64F, 0.30F),
                      .line_b = QVector3D(0.82F, 1.64F, 0.62F),
                      .line_pole = true,
                      .max_laundry = 3});
  register_home_renderer_variant(
      registry,
      HomeRendererConfig{.nation_slug = "carthage",
                         .archetype = &home_archetype,
                         .palette_slots = &home_palette_slots,
                         .selection = BuildingSelectionStyle{2.1F, 2.1F},
                         .torches = k_torches});
}

} // namespace Render::GL::Carthage
