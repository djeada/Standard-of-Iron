#include "elephant_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../elephant/rig.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/humanoid/animation/animation_inputs.h"
#include "../../../gl/humanoid/humanoid_types.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinder_between;

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

inline auto make_palette(const QVector3D &team) -> CarthageElephantPalette {
  CarthageElephantPalette p;
  p.team = clampVec01(team);
  return p;
}

inline void draw_box(ISubmitter &out, Mesh *unit, Texture *white,
                     const QMatrix4x4 &model, const QVector3D &pos,
                     const QVector3D &size, const QVector3D &color) {
  QMatrix4x4 m = model;
  m.translate(pos);
  m.scale(size);
  out.mesh(unit, m, color, white, 1.0F);
}

inline void draw_cyl(ISubmitter &out, const QMatrix4x4 &model,
                     const QVector3D &a, const QVector3D &b, float r,
                     const QVector3D &color, Texture *white) {
  out.mesh(get_unit_cylinder(), model * cylinder_between(a, b, r), color, white,
           1.0F);
}

class CarthageElephantRenderer : public ElephantRendererBase {
public:
  CarthageElephantRenderer() = default;

protected:
};

} 

void register_elephant_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/elephant", [](const DrawContext &p, ISubmitter &out) {
        static CarthageElephantRenderer const static_renderer;

        if (p.entity == nullptr) {
          return;
        }

        QVector3D team_color{0.4F, 0.2F, 0.6F};
        if (auto *r =
                p.entity->get_component<Engine::Core::RenderableComponent>()) {
          team_color = QVector3D(r->color[0], r->color[1], r->color[2]);
        }

        uint32_t seed = 0U;
        seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p.entity) &
                                     0xFFFFFFFFU);

        QVector3D const fabric_base(0.45F, 0.18F, 0.55F);
        QVector3D const metal_base(0.70F, 0.50F, 0.28F);

        ElephantProfile profile = get_or_create_cached_elephant_profile(
            seed, fabric_base, metal_base);

        AnimationInputs anim = sample_anim_state(p);

        if (p.entity != nullptr) {
          if (auto *combat_state =
                  p.entity
                      ->get_component<Engine::Core::CombatStateComponent>()) {
            if (combat_state->animation_state !=
                Engine::Core::CombatAnimationState::Idle) {
              anim.is_attacking = true;
              anim.is_melee = true;
            }
          }

          if (auto *attack =
                  p.entity->get_component<Engine::Core::AttackComponent>()) {
            if (attack->in_melee_lock) {
              anim.is_attacking = true;
              anim.is_melee = true;
            }
          }
        }

        static_renderer.render(p, anim, profile, nullptr, nullptr, out);
      });
}

} 
