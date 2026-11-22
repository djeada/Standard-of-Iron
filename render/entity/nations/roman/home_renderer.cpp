#include "render/entity/nations/roman/home_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "game/core/component.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/home_activity.h"
#include "render/entity/home_renderer_common.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/nations/roman/home_authoring.h"
#include "render/entity/registry.h"
#include "render/frame/submitter.h"
#include "render/math/math_utils.h"

namespace Render::GL::Roman {

using namespace home_detail;

namespace {

constexpr std::array<QVector3D, 5> k_wall_washes{
    QVector3D(0.91F, 0.86F, 0.73F),
    QVector3D(0.90F, 0.69F, 0.43F),
    QVector3D(0.88F, 0.62F, 0.49F),
    QVector3D(0.94F, 0.92F, 0.85F),
    QVector3D(0.79F, 0.71F, 0.54F),
};

auto home_palette_slots(const QVector3D& team, std::uint32_t seed, BuildingState state)
    -> std::array<QVector3D, k_home_palette_slots> {
  const auto palette = make_palette(team);
  QVector3D wash = k_wall_washes[home_tint_pick(seed, k_wall_washes.size())] *
                   (1.0F + home_tint_jitter(seed) * 0.06F);
  wash = decayed_color(clamp_vec_01(wash), state, static_cast<int>(seed & 0xFFFFU));
  return {palette.team, wash};
}

auto home_archetype(BuildingState state, int variant) -> const RenderArchetype& {
  static const std::array<const BuildingArchetypeSet*, k_home_visual_variant_count>
      k_sets{&building_archetype_set("roman_home"),
             &building_archetype_set("roman_home_variant_1"),
             &building_archetype_set("roman_home_variant_2"),
             &building_archetype_set("roman_home_variant_3"),
             &building_archetype_set("roman_home_variant_4"),
             &building_archetype_set("roman_home_variant_5")};
  return k_sets[static_cast<std::size_t>(variant)]->for_state(state);
}

const std::array<TorchMount, 2> k_torches{{
    TorchMount{.at = QVector3D(-0.38F, 0.6F, 0.975F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(0.38F, 0.6F, 0.975F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
}};

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry& registry) {

  register_home_smoke_anchor(
      false,
      HomeSmokeAnchor{.vent = QVector3D(0.0F, 1.66F, -0.73F),
                      .smoke_tint = QVector3D(0.56F, 0.54F, 0.51F),
                      .plume_radius = 0.68F,
                      .doorstep = QVector3D(0.0F, 0.16F, 1.24F),
                      .outward = QVector3D(0.0F, 0.0F, 1.0F),

                      .cloth_top = QVector3D(0.0F, 0.74F, 0.99F),
                      .cloth_span = 0.30F,
                      .cloth_indigo = false,
                      .cloth_at_door = true,
                      .lamp = QVector3D(0.0F, 0.165F, 1.22F),

                      .window = QVector3D(0.95F, 0.58F, 0.30F),
                      .shutter_width = 0.16F,
                      .shutter_height = 0.40F,

                      .line_a = QVector3D(-0.72F, 0.99F, 0.95F),
                      .line_b = QVector3D(-0.38F, 0.99F, 0.95F),
                      .line_pole = false,
                      .max_laundry = 2});
  register_home_renderer_variant(
      registry,
      HomeRendererConfig{.nation_slug = "roman",
                         .archetype = &home_archetype,
                         .palette_slots = &home_palette_slots,
                         .selection = BuildingSelectionStyle{2.25F, 2.25F},
                         .torches = k_torches});
}

} // namespace Render::GL::Roman
