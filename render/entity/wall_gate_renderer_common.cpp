#include "render/entity/wall_gate_renderer_common.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>

#include "game/core/component_gameplay.h"
#include "game/core/entity.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_render_common.h"
#include "render/frame/draw_resources.h"

namespace Render::GL {

namespace {

constexpr float k_jamb_offset = Engine::Core::GateComponent::k_passage_half_width;

constexpr float k_leaf_swing_degrees = 100.0F;

} // namespace

auto wall_gate_archetype(std::string_view name_prefix) -> const BuildingArchetypeSet& {
  return building_archetype_set(std::string(name_prefix) + "_gate");
}

auto wall_gate_leaf_archetype(std::string_view name_prefix)
    -> const BuildingArchetypeSet& {
  return building_archetype_set(std::string(name_prefix) + "_gate_leaf");
}

void submit_wall_gate(ISubmitter& out,
                      const DrawContext& ctx,
                      const BuildingArchetypeSet& frame,
                      const BuildingArchetypeSet& leaf) {
  const BuildingState state = resolve_building_state(ctx);
  submit_building_instance(out, ctx, frame.for_state(state));

  const auto* gate = (ctx.entity != nullptr)
                         ? ctx.entity->get_component<Engine::Core::GateComponent>()
                         : nullptr;
  const float open_amount =
      (gate != nullptr) ? std::clamp(gate->open_amount, 0.0F, 1.0F) : 0.0F;
  const float eased_open = open_amount * open_amount * (3.0F - 2.0F * open_amount);
  const float swing_degrees = eased_open * k_leaf_swing_degrees;

  for (const float side : {-1.0F, 1.0F}) {
    if (state == BuildingState::Destroyed && side < 0.0F) {
      continue;
    }
    DrawContext hinged = ctx;
    hinged.model.translate(QVector3D(side * k_jamb_offset, 0.0F, 0.0F));
    hinged.model.rotate(-side * swing_degrees, 0.0F, 1.0F, 0.0F);
    if (side < 0.0F) {
      hinged.model.rotate(180.0F, 0.0F, 1.0F, 0.0F);
    }
    if (state == BuildingState::Destroyed) {
      hinged.model.rotate(-14.0F, 0.0F, 0.0F, 1.0F);
    }
    submit_building_instance(out, hinged, leaf.for_state(state));
  }

  draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.0F, 2.0F});
}

} // namespace Render::GL
