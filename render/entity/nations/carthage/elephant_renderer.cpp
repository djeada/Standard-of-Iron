#include "elephant_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cmath>

#include "../../../entity_appearance.h"
#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/visuals/team_colors.h"
#include "math/math_utils.h"
#include "render/creature/anatomy_bake.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/entity/registry.h"
#include "render/geom/transforms.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clamp_vec_01;

struct CarthageElephantPalette {
  QVector3D fabric_purple{0.45F, 0.18F, 0.55F};
  QVector3D fabric_gold{0.85F, 0.70F, 0.35F};
  QVector3D metal_bronze{0.70F, 0.50F, 0.28F};
  QVector3D metal_gold{0.85F, 0.72F, 0.40F};
  QVector3D wood_cedar{0.52F, 0.35F, 0.22F};
  QVector3D wood_dark{0.38F, 0.25F, 0.15F};
  QVector3D leather{0.48F, 0.35F, 0.22F};
  QVector3D rope{0.58F, 0.50F, 0.38F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

class CarthageElephantRenderer : public ElephantRendererBase {
public:
  CarthageElephantRenderer()
      : ElephantRendererBase(make_visual_spec()) {}

private:
  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    Render::Creature::Pipeline::UnitVisualSpec spec;
    spec.kind = Render::Creature::Pipeline::CreatureKind::Elephant;
    spec.debug_name = "troops/carthage/elephant";
    return spec;
  }
};

} // namespace

void register_elephant_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(
      "troops/carthage/elephant", [](const DrawContext& p, ISubmitter& out) {
        static CarthageElephantRenderer const static_renderer;

        if (p.entity == nullptr) {
          return;
        }

        QVector3D team_color{0.4F, 0.2F, 0.6F};
        if (p.entity->get_component<Engine::Core::RenderableComponent>() != nullptr) {
          team_color = Render::entity_color(*p.entity);
        }

        uint32_t seed = 0U;
        seed =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p.entity) & 0xFFFFFFFFU);

        QVector3D const fabric_base(0.45F, 0.18F, 0.55F);
        QVector3D const metal_base(0.70F, 0.50F, 0.28F);

        ElephantProfile& profile = Render::Creature::get_or_bake_elephant_anatomy(
                                       p.entity, seed, fabric_base, metal_base)
                                       .profile;

        AnimationInputs const anim =
            Render::Creature::Pipeline::resolve_elephant_animation_state(p).inputs;

        static_renderer.render(p, anim, profile, nullptr, nullptr, out);
      });
}

} // namespace Render::GL::Carthage
