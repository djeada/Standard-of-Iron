#include "mounted_knight_renderer_base.h"

#include "../creature/archetype_registry.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
#include "../palette.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"

#include "mounted_knight_pose.h"
#include "renderer_constants.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Render::GL {

namespace {

constexpr QVector3D k_default_proportion_scale{0.82F, 0.90F, 0.90F};

}

MountedKnightRendererBase::MountedKnightRendererBase(
    MountedKnightRendererConfig config)
    : m_config(std::move(config)) {
  m_config.has_sword =
      m_config.has_sword && !m_config.sword_equipment_id.empty();
  if (!m_config.has_sword) {
    m_config.sword_equipment_id.clear();
  }

  m_config.has_cavalry_shield =
      m_config.has_cavalry_shield && !m_config.shield_equipment_id.empty();
  if (!m_config.has_cavalry_shield) {
    m_config.shield_equipment_id.clear();
  }

  set_mount_archetype_id(m_config.mount_archetype_id);
  build_visual_spec();
}

auto MountedKnightRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto MountedKnightRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {
  if (!m_mounted_visual_spec_baked) {
    m_mounted_visual_spec_cache =
        MountedHumanoidRendererBase::mounted_visual_spec();
    m_mounted_visual_spec_cache.rider = visual_spec();
    m_mounted_visual_spec_cache.rider.kind =
        Render::Creature::Pipeline::CreatureKind::Humanoid;
    m_mounted_visual_spec_cache.rider.debug_name =
        "troops/mounted_knight/rider";
    m_mounted_visual_spec_cache.mount.debug_name =
        "troops/mounted_knight/horse";
    m_mounted_visual_spec_baked = true;
  }
  return m_mounted_visual_spec_cache;
}

auto MountedKnightRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {
  return m_spec;
}

auto MountedKnightRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void MountedKnightRendererBase::adjust_variation(
    const DrawContext &, uint32_t, VariationParams &variation) const {
  variation.height_scale = 0.88F;
  variation.bulk_scale = 0.76F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void MountedKnightRendererBase::get_variant(const DrawContext &ctx,
                                            uint32_t seed,
                                            HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void MountedKnightRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = "troops/mounted_knight/rider";
  QVector3D const ps = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{ps.x(), ps.y(), ps.z()};
  m_spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
  m_spec.archetype_id =
      (m_config.rider_archetype_id != Render::Creature::kInvalidArchetype)
          ? m_config.rider_archetype_id
          : Render::Creature::ArchetypeRegistry::kRiderBase;
  m_spec.creature_asset_id = m_config.rider_creature_asset_id;
}

} // namespace Render::GL
