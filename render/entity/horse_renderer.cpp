#include "horse_renderer.h"

#include "render/creature/pipeline/unit_visual_spec.h"
namespace Render::GL {

namespace {

auto attached_horse_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
  Render::Creature::Pipeline::UnitVisualSpec spec;
  spec.kind = Render::Creature::Pipeline::CreatureKind::Horse;
  spec.debug_name = "horse/with_attachments";
  return spec;
}

} // namespace

HorseRenderer::HorseRenderer()
    : HorseRendererBase(attached_horse_visual_spec()) {
}

} // namespace Render::GL
