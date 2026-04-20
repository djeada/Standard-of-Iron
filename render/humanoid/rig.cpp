#include "cache_control.h"
#include "humanoid_renderer_base.h"
#include "mesh_helpers.h"
#include "prepare.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../entity/registry.h"
#include "../geom/parts.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../palette.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <memory>

namespace Render::GL {

auto torso_mesh_without_bottom_cap() -> Mesh * {
  static std::unique_ptr<Mesh> s_mesh;
  if (s_mesh != nullptr) {
    return s_mesh.get();
  }

  Mesh *base = get_unit_torso();
  if (base == nullptr) {
    return nullptr;
  }

  auto filtered = base->clone_with_filtered_indices(
      [](unsigned int a, unsigned int b, unsigned int c,
         const std::vector<Vertex> &verts) -> bool {
        auto sample = [&](unsigned int idx) -> QVector3D {
          return {verts[idx].position[0], verts[idx].position[1],
                  verts[idx].position[2]};
        };
        QVector3D pa = sample(a);
        QVector3D pb = sample(b);
        QVector3D pc = sample(c);
        float min_y = std::min({pa.y(), pb.y(), pc.y()});
        float max_y = std::max({pa.y(), pb.y(), pc.y()});

        QVector3D n(
            verts[a].normal[0] + verts[b].normal[0] + verts[c].normal[0],
            verts[a].normal[1] + verts[b].normal[1] + verts[c].normal[1],
            verts[a].normal[2] + verts[b].normal[2] + verts[c].normal[2]);
        if (n.lengthSquared() > 0.0F) {
          n.normalize();
        }

        constexpr float k_band_height = 0.02F;
        constexpr float k_bottom_threshold = 0.45F;
        bool is_flat = (max_y - min_y) < k_band_height;
        bool is_at_bottom = min_y > k_bottom_threshold;
        bool facing_down = (n.y() > 0.35F);
        return is_flat && is_at_bottom && facing_down;
      });

  s_mesh =
      (filtered != nullptr) ? std::move(filtered) : std::unique_ptr<Mesh>(base);
  return s_mesh.get();
}

void align_torso_mesh_forward(QMatrix4x4 &model) noexcept {
  model.rotate(90.0F, 0.0F, 1.0F, 0.0F);
}

auto HumanoidRendererBase::frame_local_position(
    const AttachmentFrame &frame, const QVector3D &local) -> QVector3D {
  float const lx = local.x() * frame.radius;
  float const ly = local.y() * frame.radius;
  float const lz = local.z() * frame.radius;
  return frame.origin + frame.right * lx + frame.up * ly + frame.forward * lz;
}

auto HumanoidRendererBase::make_frame_local_transform(
    const QMatrix4x4 &parent, const AttachmentFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  float scale = frame.radius * uniform_scale;
  if (scale == 0.0F) {
    scale = uniform_scale;
  }

  QVector3D const origin = frame_local_position(frame, local_offset);

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(frame.right * scale, 0.0F));
  local.setColumn(1, QVector4D(frame.up * scale, 0.0F));
  local.setColumn(2, QVector4D(frame.forward * scale, 0.0F));
  local.setColumn(3, QVector4D(origin, 1.0F));
  return parent * local;
}

auto HumanoidRendererBase::head_local_position(
    const HeadFrame &frame, const QVector3D &local) -> QVector3D {
  return frame_local_position(frame, local);
}

auto HumanoidRendererBase::make_head_local_transform(
    const QMatrix4x4 &parent, const HeadFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  return make_frame_local_transform(parent, frame, local_offset, uniform_scale);
}

void HumanoidRendererBase::get_variant(const DrawContext &ctx, uint32_t seed,
                                       HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void HumanoidRendererBase::customize_pose(const DrawContext &,
                                          const HumanoidAnimationContext &,
                                          uint32_t, HumanoidPose &) const {}

void HumanoidRendererBase::add_attachments(const DrawContext &,
                                           const HumanoidVariant &,
                                           const HumanoidPose &,
                                           const HumanoidAnimationContext &,
                                           ISubmitter &) const {}

auto HumanoidRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {

  static thread_local Render::Creature::Pipeline::UnitVisualSpec spec;
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "humanoid/default";
  spec.equipment = {};
  spec.pose_hook = nullptr;
  spec.variant_hook = nullptr;
  const QVector3D ps = get_proportion_scaling();
  spec.scaling =
      Render::Creature::Pipeline::ProportionScaling{ps.x(), ps.y(), ps.z()};
  return spec;
}

auto HumanoidRendererBase::resolve_entity_ground_offset(
    const DrawContext &, Engine::Core::UnitComponent *unit_comp,
    Engine::Core::TransformComponent *transform_comp) const -> float {
  (void)unit_comp;
  (void)transform_comp;

  return 0.0F;
}

auto HumanoidRendererBase::resolve_team_tint(const DrawContext &ctx)
    -> QVector3D {
  QVector3D tunic(0.8F, 0.9F, 1.0F);
  Engine::Core::UnitComponent *unit = nullptr;
  Engine::Core::RenderableComponent *rc = nullptr;

  if (ctx.entity != nullptr) {
    unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    rc = ctx.entity->get_component<Engine::Core::RenderableComponent>();
  }

  if ((unit != nullptr) && unit->owner_id > 0) {
    tunic = Game::Visuals::team_colorForOwner(unit->owner_id);
  } else if (rc != nullptr) {
    tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
  }

  return tunic;
}

auto HumanoidRendererBase::resolve_formation(const DrawContext &ctx)
    -> FormationParams {
  FormationParams params{};
  params.individuals_per_unit = 1;
  params.max_per_row = 1;
  params.spacing = 0.75F;

  if (ctx.entity != nullptr) {
    auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      params.individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->spawn_type);
      params.max_per_row =
          Game::Units::TroopConfig::instance().getMaxUnitsPerRow(
              unit->spawn_type);
      if (unit->spawn_type == Game::Units::SpawnType::MountedKnight) {
        params.spacing = 1.05F;
      }
    }
  }

  return params;
}

void HumanoidRendererBase::render(const DrawContext &ctx,
                                  ISubmitter &out) const {
  AnimationInputs anim = sample_anim_state(ctx);

  if (ctx.template_prewarm) {
    // Template-prewarm contexts are flagged as RenderPassIntent::Shadow by
    // pass_intent_from_ctx; PreparedCreatureSubmitBatch::submit drops Shadow
    // rows. We still early-return here because render_procedural also runs
    // post_body_draws (direct ISubmitter emissions) and pose/animation cache
    // mutations that are not gated by the Shadow filter. Removing this guard
    // would record extra draws into the prewarm TemplateRecorder.
    return;
  }

  render_procedural(ctx, anim, out);
}

void HumanoidRendererBase::render_procedural(const DrawContext &ctx,
                                             const AnimationInputs &anim,
                                             ISubmitter &out) const {
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(*this, ctx, anim,
                                               humanoid_current_frame(), prep);

  Render::Creature::Pipeline::submit_preparation(prep, out);
}

} // namespace Render::GL
