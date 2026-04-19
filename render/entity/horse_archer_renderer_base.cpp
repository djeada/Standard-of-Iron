#include "horse_archer_renderer_base.h"

#include "../creature/pipeline/equipment_registry.h"
#include "../equipment/armor/cloak_renderer.h"
#include "../equipment/equipment_registry.h"
#include "../equipment/equipment_submit.h"
#include "../equipment/weapons/bow_renderer.h"
#include "../equipment/weapons/quiver_renderer.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
#include "../humanoid/mounted_pose_controller.h"
#include "../palette.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/systems/nation_id.h"

#include "mounted_knight_pose.h"
#include "renderer_constants.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Render::GL {

namespace {

constexpr QVector3D k_default_proportion_scale{0.80F, 0.88F, 0.88F};

}

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

  m_horseRenderer.set_attachments(m_config.horse_attachments);

  cache_equipment();
  build_visual_spec();
}

auto HorseArcherRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto HorseArcherRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {
  static thread_local Render::Creature::Pipeline::MountedSpec spec;
  spec = MountedHumanoidRendererBase::mounted_visual_spec();
  spec.rider = visual_spec();
  spec.rider.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.rider.debug_name = "troops/horse_archer/rider";
  spec.mount.debug_name = "troops/horse_archer/horse";
  return spec;
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

void HorseArcherRendererBase::apply_riding_animation(
    MountedPoseController &mounted_controller, MountedAttachmentFrame &mount,
    const HumanoidAnimationContext &anim_ctx, HumanoidPose &pose,
    const HorseDimensions &dims, const ReinState &reins) const {
  (void)pose;
  (void)dims;
  (void)reins;
  const AnimationInputs &anim = anim_ctx.inputs;
  if (anim.is_attacking && !anim.is_melee) {
    float const attack_phase =
        std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
    mounted_controller.riding_bow_shot(mount, attack_phase);
  } else {
    mounted_controller.riding_idle(mount);
  }
}

void HorseArcherRendererBase::cache_equipment() {
  auto &registry = EquipmentRegistry::instance();

  if (!m_config.bow_equipment_id.empty()) {
    m_cached_bow =
        registry.get(EquipmentCategory::Weapon, m_config.bow_equipment_id);
  }

  if (!m_config.quiver_equipment_id.empty()) {
    m_cached_quiver =
        registry.get(EquipmentCategory::Weapon, m_config.quiver_equipment_id);
  }

  if (!m_config.helmet_equipment_id.empty()) {
    m_cached_helmet =
        registry.get(EquipmentCategory::Helmet, m_config.helmet_equipment_id);
  }

  if (!m_config.armor_equipment_id.empty()) {
    m_cached_armor =
        registry.get(EquipmentCategory::Armor, m_config.armor_equipment_id);
  }

  if (!m_config.cloak_equipment_id.empty()) {
    m_cached_cloak =
        registry.get(EquipmentCategory::Armor, m_config.cloak_equipment_id);
  }
}

void HorseArcherRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  m_loadout.clear();

  if (m_cached_helmet) {
    auto *helmet_ptr = m_cached_helmet.get();
    float const helmet_offset = m_config.helmet_offset_moving;
    EquipmentRecord rec{};
    rec.dispatch = [helmet_ptr,
                    helmet_offset](const EquipmentSubmitContext &sub,
                                   Render::GL::EquipmentBatch &batch) {
      if (helmet_ptr == nullptr || sub.ctx == nullptr ||
          sub.frames == nullptr || sub.palette == nullptr) {
        return;
      }
      BodyFrames frames = *sub.frames;
      if (sub.ctx->entity != nullptr && helmet_offset > 0.0F) {
        if (auto *move = sub.ctx->entity
                             ->get_component<Engine::Core::MovementComponent>();
            move != nullptr) {
          float const speed_sq = move->vx * move->vx + move->vz * move->vz;
          if (speed_sq > 0.0001F) {
            frames.head.origin += frames.head.forward * helmet_offset;
          }
        }
      }
      HumanoidAnimationContext anim_ctx{};
      helmet_ptr->render(*sub.ctx, frames, *sub.palette, anim_ctx, batch);
    };
    m_loadout.push_back(std::move(rec));
  }

  if (m_cached_armor) {
    m_loadout.push_back(make_legacy_equipment_record(*m_cached_armor));
  }

  if (m_config.has_cloak && m_cached_cloak) {
    auto *cloak_ptr = m_cached_cloak.get();
    QVector3D const cloak_color = m_config.cloak_color;
    QVector3D const cloak_trim_color = m_config.cloak_trim_color;
    int const back_material_id = m_config.cloak_back_material_id;
    int const shoulder_material_id = m_config.cloak_shoulder_material_id;
    EquipmentRecord rec{};
    rec.dispatch = [cloak_ptr, cloak_color, cloak_trim_color, back_material_id,
                    shoulder_material_id](const EquipmentSubmitContext &sub,
                                          Render::GL::EquipmentBatch &batch) {
      if (cloak_ptr == nullptr || sub.ctx == nullptr || sub.frames == nullptr ||
          sub.palette == nullptr || sub.anim == nullptr) {
        return;
      }
      if (auto *cr = dynamic_cast<CloakRenderer *>(cloak_ptr)) {
        CloakConfig cfg;
        cfg.primary_color = cloak_color;
        cfg.trim_color = cloak_trim_color;
        cfg.back_material_id = back_material_id;
        cfg.shoulder_material_id = shoulder_material_id;
        cr->set_config(cfg);
      }
      cloak_ptr->render(*sub.ctx, *sub.frames, *sub.palette, *sub.anim, batch);
    };
    m_loadout.push_back(std::move(rec));
  }

  if (m_config.has_bow) {
    auto *bow_ptr = m_cached_bow.get();
    QVector3D const metal_color = m_config.metal_color;
    QVector3D const fletching_color = m_config.fletching_color;
    EquipmentRecord rec{};
    rec.dispatch = [bow_ptr, metal_color,
                    fletching_color](const EquipmentSubmitContext &sub,
                                     Render::GL::EquipmentBatch &batch) {
      if (sub.ctx == nullptr || sub.frames == nullptr ||
          sub.palette == nullptr || sub.anim == nullptr) {
        return;
      }
      auto *bow_renderer = dynamic_cast<BowRenderer *>(bow_ptr);
      BowRenderConfig bow_config =
          bow_renderer ? bow_renderer->base_config() : BowRenderConfig{};
      bow_config.string_color = QVector3D(0.30F, 0.30F, 0.32F);
      bow_config.metal_color = metal_color;
      bow_config.fletching_color = fletching_color;
      bow_config.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
      bow_config.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
      bow_config.bow_x = 0.0F;
      BowRenderer::submit(bow_config, *sub.ctx, *sub.frames, *sub.palette,
                          *sub.anim, batch);
    };
    m_loadout.push_back(std::move(rec));
  }

  if (m_config.has_quiver && m_cached_quiver) {
    auto *quiver_ptr = m_cached_quiver.get();
    QVector3D const fletching_color = m_config.fletching_color;
    EquipmentRecord rec{};
    rec.dispatch = [quiver_ptr,
                    fletching_color](const EquipmentSubmitContext &sub,
                                     Render::GL::EquipmentBatch &batch) {
      if (quiver_ptr == nullptr || sub.ctx == nullptr ||
          sub.frames == nullptr || sub.palette == nullptr ||
          sub.anim == nullptr) {
        return;
      }
      QuiverRenderConfig quiver_config;
      quiver_config.fletching_color = fletching_color;
      quiver_config.quiver_radius = HumanProportions::HEAD_RADIUS * 0.45F;
      if (auto *qr = dynamic_cast<QuiverRenderer *>(quiver_ptr)) {
        qr->set_config(quiver_config);
      }
      quiver_ptr->render(*sub.ctx, *sub.frames, *sub.palette, *sub.anim, batch);
    };
    m_loadout.push_back(std::move(rec));
  }

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = "troops/horse_archer/rider";
  QVector3D const ps = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{ps.x(), ps.y(), ps.z()};
  m_spec.equipment =
      std::span<const EquipmentRecord>{m_loadout.data(), m_loadout.size()};
  m_spec.owned_legacy_slots = LegacySlotMask::Helmet | LegacySlotMask::Armor;
}

auto HorseArcherRendererBase::resolve_shader_key(const DrawContext &ctx) const
    -> QString {
  std::string nation;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation = Game::Systems::nation_id_to_string(unit->nation_id);
    }
  }
  if (!nation.empty()) {
    return QString::fromStdString(std::string("horse_archer_") + nation);
  }
  return QStringLiteral("horse_archer");
}

} // namespace Render::GL
