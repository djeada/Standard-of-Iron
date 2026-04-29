#include "archer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/creature_render_graph.h"
#include "../../../creature/pipeline/preparation_common.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/cloak_renderer.h"
#include "../../../equipment/armor/roman_armor.h"
#include "../../../equipment/armor/roman_greaves.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../equipment/helmets/roman_light_helmet.h"
#include "../../../equipment/weapons/bow_renderer.h"
#include "../../../equipment/weapons/quiver_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_full_builder.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/pose_controller.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "archer_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr std::string_view k_attachment_headwrap = "carthage_headwrap";

constexpr float k_kneel_depth_multiplier = 1.125F;
constexpr float k_lean_amount_multiplier = 0.83F;

auto style_registry() -> std::unordered_map<std::string, ArcherStyleConfig> & {
  static std::unordered_map<std::string, ArcherStyleConfig> styles;
  return styles;
}

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_roman_archer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

auto canonical_bow_config() -> BowRenderConfig {
  BowRenderConfig cfg;
  cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
  cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
  cfg.bow_x = 0.0F;
  return cfg;
}

} // namespace

void register_archer_style(const std::string &nation_id,
                           const ArcherStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class ArcherRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.78F, 1.01F, 0.96F};
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;

    static const UnitVisualSpec spec = []() {
      static const auto k_head_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
      static const auto k_foot_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootL);
      static const auto k_foot_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootR);
      static const auto k_helmet_base_role_byte = static_cast<std::uint8_t>(
          Render::Humanoid::k_humanoid_role_count + 1U);
      static const auto k_greaves_base_role_byte = static_cast<std::uint8_t>(
          k_helmet_base_role_byte + Render::GL::kRomanLightHelmetRoleCount);
      static const auto k_quiver_base_role_byte = static_cast<std::uint8_t>(
          k_greaves_base_role_byte + Render::GL::kRomanGreavesRoleCount);
      static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
          k_quiver_base_role_byte + Render::GL::kQuiverRoleCount);
      static const auto k_bow_base_role_byte = static_cast<std::uint8_t>(
          k_armor_base_role_byte + Render::GL::kRomanLightArmorRoleCount);
      static const auto k_pelvis_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Pelvis);
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_head_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::Head)];
      static const auto k_shin_l_bind_matrix = []() -> QMatrix4x4 {
        const auto &f = Render::Humanoid::humanoid_bind_body_frames().shin_l;
        QMatrix4x4 m;
        m.setColumn(0, QVector4D(f.right, 0.0F));
        m.setColumn(1, QVector4D(f.up, 0.0F));
        m.setColumn(2, QVector4D(f.forward, 0.0F));
        m.setColumn(3, QVector4D(f.origin, 1.0F));
        return m;
      }();
      static const auto k_shin_r_bind_matrix = []() -> QMatrix4x4 {
        const auto &f = Render::Humanoid::humanoid_bind_body_frames().shin_r;
        QMatrix4x4 m;
        m.setColumn(0, QVector4D(f.right, 0.0F));
        m.setColumn(1, QVector4D(f.up, 0.0F));
        m.setColumn(2, QVector4D(f.forward, 0.0F));
        m.setColumn(3, QVector4D(f.origin, 1.0F));
        return m;
      }();
      static const Render::Creature::StaticAttachmentSpec k_helmet_spec =
          Render::GL::roman_light_helmet_make_static_attachment(
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_greaves_l_spec =
          Render::GL::roman_greaves_make_static_attachment(
              k_foot_l_bone, k_greaves_base_role_byte, k_shin_l_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_greaves_r_spec =
          Render::GL::roman_greaves_make_static_attachment(
              k_foot_r_bone, k_greaves_base_role_byte, k_shin_r_bind_matrix);
      static const std::array<Render::Creature::StaticAttachmentSpec, 5>
          k_quiver_specs = Render::GL::quiver_make_static_attachments(
              QuiverRenderConfig{}, k_pelvis_bone, k_quiver_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 2>
          k_bow_specs = Render::GL::bow_make_static_attachments(
              canonical_bow_config(), k_bow_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_armor_spec =
          Render::GL::roman_light_armor_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte);
      static const auto k_cloak_base_role_byte = static_cast<std::uint8_t>(
          k_bow_base_role_byte + Render::GL::kBowRoleCount);
      static const auto k_torso_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const CloakConfig k_cloak_cfg = []() {
        CloakConfig cfg;
        cfg.primary_color = QVector3D(0.70F, 0.15F, 0.18F);
        return cfg;
      }();
      static const CloakMeshes k_cloak_meshes = []() -> CloakMeshes {
        auto &reg = Render::GL::EquipmentRegistry::instance();
        auto cloak_inst =
            reg.get(Render::GL::EquipmentCategory::Armor, "cloak_carthage");
        if (cloak_inst) {
          if (auto *cr = dynamic_cast<CloakRenderer *>(cloak_inst.get())) {
            return cr->meshes();
          }
        }
        return CloakMeshes{};
      }();
      static const Render::Creature::StaticAttachmentSpec k_cloak_spec =
          Render::GL::cloak_make_static_attachment(k_cloak_cfg, k_cloak_meshes,
                                                   k_torso_bone,
                                                   k_cloak_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 12>
          k_attachments{k_helmet_spec,     k_greaves_l_spec,  k_greaves_r_spec,
                        k_quiver_specs[0], k_quiver_specs[1], k_quiver_specs[2],
                        k_quiver_specs[3], k_quiver_specs[4], k_armor_spec,
                        k_bow_specs[0],    k_bow_specs[1],    k_cloak_spec};
      static const auto k_archer_archetype = []() {
        using Render::Creature::AnimationStateId;
        using Render::Creature::ArchetypeDescriptor;
        using Render::Creature::ArchetypeRegistry;

        auto &registry = ArchetypeRegistry::instance();
        auto const *base_desc = registry.get(ArchetypeRegistry::kHumanoidBase);
        if (base_desc == nullptr) {
          return Render::Creature::kInvalidArchetype;
        }

        ArchetypeDescriptor desc = *base_desc;
        desc.debug_name = "troops/roman/archer";
        desc.bake_attachments = {};
        for (std::size_t i = 0; i < k_attachments.size(); ++i) {
          desc.bake_attachments[i] = k_attachments[i];
        }
        desc.bake_attachment_count =
            static_cast<std::uint8_t>(k_attachments.size());
        desc.extra_role_color_fns = {};
        desc.extra_role_color_fn_count = 0;
        desc.append_extra_role_colors_fn(
            +[](const void *variant_void, QVector3D *out,
                std::uint32_t base_count,
                std::size_t max_count) -> std::uint32_t {
              if (variant_void == nullptr) {
                return base_count;
              }
              if (max_count <= base_count) {
                return base_count;
              }
              const auto &v =
                  *static_cast<const HumanoidVariant *>(variant_void);
              auto count = base_count;
              count += Render::GL::roman_light_helmet_fill_role_colors(
                  v.palette, out + count, max_count - count);
              if (max_count <= count) {
                return count;
              }
              count += Render::GL::roman_greaves_fill_role_colors(
                  v.palette, out + count, max_count - count);
              if (max_count <= count) {
                return count;
              }
              static const QuiverRenderConfig k_quiver_cfg{};
              count += Render::GL::quiver_fill_role_colors(
                  v.palette, k_quiver_cfg, out + count, max_count - count);
              if (max_count <= count) {
                return count;
              }
              count += Render::GL::roman_light_armor_fill_role_colors(
                  v.palette, out + count, max_count - count);
              if (max_count <= count) {
                return count;
              }
              count += Render::GL::bow_fill_role_colors(v.palette, out + count,
                                                        max_count - count);
              if (max_count <= count) {
                return count;
              }
              count += Render::GL::cloak_fill_role_colors_with_primary(
                  QVector3D(0.70F, 0.15F, 0.18F), v.palette, out + count,
                  max_count - count);
              return count;
            });

        auto const attack_bow_clip = desc.bpat_clip[static_cast<std::size_t>(
            AnimationStateId::AttackBow)];
        desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Idle)] =
            attack_bow_clip;
        desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Hold)] =
            attack_bow_clip;
        return registry.register_archetype(desc);
      }();

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/roman/archer";
      s.scaling = ProportionScaling{0.78F, 1.01F, 0.96F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = k_archer_archetype;
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);
  }

  void append_companion_preparation(
      const DrawContext &ctx, const HumanoidVariant &variant,
      const HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx,
      std::uint32_t, Render::Creature::CreatureLOD,
      Render::Creature::Pipeline::CreaturePreparationResult &out)
      const override {
    (void)ctx;
    (void)variant;
    (void)pose;
    (void)anim_ctx;
    (void)out;
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const ArcherStyleConfig & {
    ensure_archer_styles_registered();
    auto &styles = style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->get_component<Engine::Core::UnitComponent>()) {
        nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
      }
    }
    if (!nation_id.empty()) {
      auto it = styles.find(nation_id);
      if (it != styles.end()) {
        return it->second;
      }
    }
    auto fallback = styles.find(std::string(k_default_style_key));
    if (fallback != styles.end()) {
      return fallback->second;
    }
    static const ArcherStyleConfig default_style{};
    return default_style;
  }

private:
  void apply_palette_overrides(const ArcherStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }
};

void register_archer_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_archer_styles_registered();
  static ArcherRenderer const renderer;
  registry.register_renderer("troops/roman/archer",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static ArcherRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
