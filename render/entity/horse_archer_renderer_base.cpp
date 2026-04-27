#include "horse_archer_renderer_base.h"

#include "../creature/archetype_registry.h"
#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/preparation_common.h"
#include "../equipment/equipment_submit.h"
#include "../equipment/weapons/bow_renderer.h"
#include "../equipment/weapons/quiver_renderer.h"
#include "../humanoid/humanoid_full_builder.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_spec.h"
#include "../humanoid/humanoid_specs.h"
#include "../humanoid/mounted_pose_controller.h"
#include "../humanoid/skeleton.h"
#include "../palette.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"

#include "mounted_knight_pose.h"
#include "renderer_constants.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <span>
#include <tuple>
#include <utility>

namespace Render::GL {

namespace {

constexpr QVector3D k_default_proportion_scale{0.82F, 0.90F, 0.90F};

auto canonical_bow_config(const HorseArcherRendererConfig &config)
    -> BowRenderConfig {
  BowRenderConfig bow_cfg;
  bow_cfg.metal_color = config.metal_color;
  bow_cfg.fletching_color = config.fletching_color;
  bow_cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
  bow_cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
  bow_cfg.bow_x = 0.0F;
  return bow_cfg;
}

auto horse_archer_extra_role_colors(const void *variant_void, QVector3D *out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr) {
    return base_count;
  }
  const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
  auto count = base_count;

  if (count < max_count) {
    count += bow_fill_role_colors(v.palette, out + count, max_count - count);
  }
  if (count < max_count) {
    QuiverRenderConfig quiver_cfg;
    quiver_cfg.fletching_color = QVector3D(0.85F, 0.40F, 0.40F);
    count += quiver_fill_role_colors(v.palette, quiver_cfg, out + count,
                                     max_count - count);
  }
  return count;
}

struct ArchetypeCacheKey {
  Render::Creature::ArchetypeId base_id;
  bool has_bow;
  bool has_quiver;

  auto operator<(const ArchetypeCacheKey &other) const -> bool {
    return std::tie(base_id, has_bow, has_quiver) <
           std::tie(other.base_id, other.has_bow, other.has_quiver);
  }
};

auto archetype_cache()
    -> std::map<ArchetypeCacheKey, Render::Creature::ArchetypeId> & {
  static std::map<ArchetypeCacheKey, Render::Creature::ArchetypeId> cache;
  return cache;
}

auto archetype_cache_mutex() -> std::mutex & {
  static std::mutex m;
  return m;
}

} // namespace

HorseArcherRendererBase::HorseArcherRendererBase(
    HorseArcherRendererConfig config)
    : m_config(std::move(config)) {
  m_config.has_bow = m_config.has_bow && !m_config.bow_equipment_id.empty();
  if (!m_config.has_bow) {
    m_config.bow_equipment_id.clear();
  }

  m_config.has_quiver =
      m_config.has_quiver && !m_config.quiver_equipment_id.empty();
  if (!m_config.has_quiver) {
    m_config.quiver_equipment_id.clear();
  }

  set_mount_archetype_id(m_config.mount_archetype_id);

  build_visual_spec();
}

auto HorseArcherRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto HorseArcherRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {
  if (!m_mounted_visual_spec_baked) {
    m_mounted_visual_spec_cache =
        MountedHumanoidRendererBase::mounted_visual_spec();
    m_mounted_visual_spec_cache.rider = visual_spec();
    m_mounted_visual_spec_cache.rider.kind =
        Render::Creature::Pipeline::CreatureKind::Humanoid;
    Render::Creature::ArchetypeId const rider_id =
        m_rider_archetype_with_bow != Render::Creature::kInvalidArchetype
            ? m_rider_archetype_with_bow
            : (m_config.rider_archetype_id !=
                       Render::Creature::kInvalidArchetype
                   ? m_config.rider_archetype_id
                   : Render::Creature::ArchetypeRegistry::kRiderBase);
    m_mounted_visual_spec_cache.rider.archetype_id = rider_id;
    m_mounted_visual_spec_cache.rider.debug_name = "troops/horse_archer/rider";
    m_mounted_visual_spec_cache.mount.debug_name = "troops/horse_archer/horse";
    m_mounted_visual_spec_baked = true;
  }
  return m_mounted_visual_spec_cache;
}

auto HorseArcherRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {
  return m_spec;
}

auto HorseArcherRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void HorseArcherRendererBase::adjust_variation(
    const DrawContext &, uint32_t, VariationParams &variation) const {
  variation.height_scale = 0.88F;
  variation.bulk_scale = 0.72F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void HorseArcherRendererBase::get_variant(const DrawContext &ctx, uint32_t seed,
                                          HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void HorseArcherRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;
  using Render::Creature::ArchetypeRegistry;

  Render::Creature::ArchetypeId const base_rider_id =
      m_config.rider_archetype_id != Render::Creature::kInvalidArchetype
          ? m_config.rider_archetype_id
          : ArchetypeRegistry::kRiderBase;

  if (m_config.has_bow || m_config.has_quiver) {
    ArchetypeCacheKey const key{base_rider_id, m_config.has_bow,
                                m_config.has_quiver};
    {
      std::lock_guard<std::mutex> lock(archetype_cache_mutex());
      auto it = archetype_cache().find(key);
      if (it != archetype_cache().end()) {
        m_rider_archetype_with_bow = it->second;
      }
    }

    if (m_rider_archetype_with_bow == Render::Creature::kInvalidArchetype) {
      auto const *base_desc = ArchetypeRegistry::instance().get(base_rider_id);
      if (base_desc != nullptr) {
        Render::Creature::ArchetypeDescriptor desc = *base_desc;
        desc.debug_name = "troops/horse_archer/rider";

        auto base_role_byte = static_cast<std::uint8_t>(desc.role_count);

        if (m_config.has_bow) {
          auto const bow_specs = bow_make_static_attachments(
              canonical_bow_config(m_config), base_role_byte);
          for (auto const &spec : bow_specs) {
            if (desc.bake_attachment_count >=
                Render::Creature::ArchetypeDescriptor::kMaxBakeAttachments) {
              break;
            }
            desc.bake_attachments[desc.bake_attachment_count++] = spec;
          }
          base_role_byte =
              static_cast<std::uint8_t>(base_role_byte + kBowRoleCount);
        }

        if (m_config.has_quiver) {
          auto const pelvis_bone = static_cast<std::uint16_t>(
              Render::Humanoid::HumanoidBone::Pelvis);
          QuiverRenderConfig quiver_cfg;
          quiver_cfg.fletching_color = m_config.fletching_color;
          quiver_cfg.quiver_radius = HumanProportions::HEAD_RADIUS * 0.45F;
          auto const quiver_specs = quiver_make_static_attachments(
              quiver_cfg, pelvis_bone, base_role_byte);
          for (auto const &spec : quiver_specs) {
            if (desc.bake_attachment_count >=
                Render::Creature::ArchetypeDescriptor::kMaxBakeAttachments) {
              break;
            }
            desc.bake_attachments[desc.bake_attachment_count++] = spec;
          }
          base_role_byte =
              static_cast<std::uint8_t>(base_role_byte + kQuiverRoleCount);
        }

        desc.role_count = base_role_byte;
        desc.append_extra_role_colors_fn(&horse_archer_extra_role_colors);

        auto const new_id =
            ArchetypeRegistry::instance().register_archetype(desc);
        if (new_id != Render::Creature::kInvalidArchetype) {
          std::lock_guard<std::mutex> lock(archetype_cache_mutex());
          archetype_cache()[key] = new_id;
        }
        m_rider_archetype_with_bow = new_id;
      }
    }
  }

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = "troops/horse_archer/rider";
  QVector3D const ps = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{ps.x(), ps.y(), ps.z()};
  m_spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
  m_spec.archetype_id =
      m_rider_archetype_with_bow != Render::Creature::kInvalidArchetype
          ? m_rider_archetype_with_bow
          : base_rider_id;
}

void HorseArcherRendererBase::append_companion_preparation(
    const DrawContext &ctx, const HumanoidVariant &variant,
    const HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx,
    std::uint32_t seed, Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::CreaturePreparationResult &out) const {
  MountedHumanoidRendererBase::append_companion_preparation(
      ctx, variant, pose, anim_ctx, seed, lod, out);
}

} // namespace Render::GL
