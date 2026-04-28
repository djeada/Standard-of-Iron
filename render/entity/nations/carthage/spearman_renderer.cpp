#include "spearman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/armor_light_carthage.h"
#include "../../../equipment/armor/carthage_shoulder_cover.h"
#include "../../../equipment/armor/shoulder_cover_archetype.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../equipment/helmets/carthage_heavy_helmet.h"
#include "../../../equipment/weapons/spear_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/spear_pose_utils.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "spearman_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_spearman_default_style_key = "default";
constexpr float k_spearman_team_mix_weight = 0.6F;
constexpr float k_spearman_style_mix_weight = 0.4F;

constexpr float k_kneel_depth_multiplier = 0.875F;
constexpr float k_lean_amount_multiplier = 0.67F;

auto spearman_style_registry()
    -> std::unordered_map<std::string, SpearmanStyleConfig> & {
  static std::unordered_map<std::string, SpearmanStyleConfig> styles;
  return styles;
}

void ensure_spearman_styles_registered() {
  static const bool registered = []() {
    register_carthage_spearman_style();
    return true;
  }();
  (void)registered;
}

auto resolve_spearman_style_fn(const Render::GL::DrawContext &ctx)
    -> const SpearmanStyleConfig & {
  ensure_spearman_styles_registered();
  auto &styles = spearman_style_registry();
  std::string nation_id;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
    }
  }
  if (!nation_id.empty()) {
    auto it = styles.find(nation_id);
    if (it != styles.end()) {
      return it->second;
    }
  }
  auto it_default = styles.find(std::string(k_spearman_default_style_key));
  if (it_default != styles.end()) {
    return it_default->second;
  }
  static const SpearmanStyleConfig k_empty{};
  return k_empty;
}

} // namespace

void register_spearman_style(const std::string &nation_id,
                             const SpearmanStyleConfig &style) {
  spearman_style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::ease_in_out_cubic;
using Render::Geom::lerp;
using Render::Geom::smoothstep;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

struct SpearmanExtras {
  QVector3D spear_shaft_color;
  QVector3D spearhead_color;
  float spear_length = 1.20F;
  float spear_shaft_radius = 0.020F;
  float spearhead_length = 0.18F;
};

class SpearmanRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.72F, 1.02F, 0.74F};
  }

  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override {
    variation.bulk_scale *= 0.90F;
    variation.stance_width *= 0.92F;
  }

public:
  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;

    ensure_spearman_styles_registered();
    static const UnitVisualSpec spec = []() {
      static const auto k_head_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
      static const auto k_helmet_base_role_byte =
          static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
      static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
          k_helmet_base_role_byte + Render::GL::kCarthageHeavyHelmetRoleCount);
      static const auto k_spear_base_role_byte = static_cast<std::uint8_t>(
          k_shoulder_base_role_byte +
          Render::GL::kCarthageShoulderCoverRoleCount);
      static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
          k_spear_base_role_byte + Render::GL::kSpearRoleCount);
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_shoulder_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
      static const auto k_shoulder_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
      static const auto k_head_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::Head)];
      const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
      static const auto k_shoulder_l_bind_matrix =
          Render::GL::make_shoulder_cover_transform(
              QMatrix4x4{}, bind_frames.shoulder_l.origin,
              -bind_frames.torso.right, bind_frames.torso.up);
      static const auto k_shoulder_r_bind_matrix =
          Render::GL::make_shoulder_cover_transform(
              QMatrix4x4{}, bind_frames.shoulder_r.origin,
              bind_frames.torso.right, bind_frames.torso.up);
      static const Render::Creature::StaticAttachmentSpec k_shoulder_l_spec =
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_l_bone, k_shoulder_base_role_byte,
              k_shoulder_l_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_shoulder_r_spec =
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_r_bone, k_shoulder_base_role_byte,
              k_shoulder_r_bind_matrix);
      static const auto k_hand_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandR);
      static const auto k_hand_r_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::HandR)];
      static const QVector3D k_hand_r_bind_right = bind_frames.hand_r.right;
      static const SpearRenderConfig k_canonical_spear_cfg = []() {
        SpearRenderConfig cfg;
        cfg.shaft_color =
            QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
        cfg.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
        cfg.spear_length = 1.15F;
        cfg.shaft_radius = 0.018F;
        cfg.spearhead_length = 0.16F;
        return cfg;
      }();
      static const std::array<Render::Creature::StaticAttachmentSpec, 4>
          k_spear_specs = Render::GL::spear_make_static_attachments(
              k_canonical_spear_cfg, k_spear_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_armor_spec =
          Render::GL::armor_light_carthage_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 13>
          k_attachments{
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_shell_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_neck_guard_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_cheek_guards_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_face_plate_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_crest_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_rivets_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              k_shoulder_l_spec,
              k_shoulder_r_spec,
              k_spear_specs[0],
              k_spear_specs[1],
              k_spear_specs[2],
              k_spear_specs[3],
              k_armor_spec,
          };
      static const auto k_archetype =
          Render::Creature::ArchetypeRegistry::instance()
              .register_unit_archetype(
                  "troops/carthage/spearman", CreatureKind::Humanoid,
                  std::span<const Render::Creature::StaticAttachmentSpec>(
                      k_attachments.data(), k_attachments.size()),
                  +[](const void *variant_void, QVector3D *out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t {
                    if (variant_void == nullptr || max_count <= base_count) {
                      return base_count;
                    }
                    const auto &v =
                        *static_cast<const HumanoidVariant *>(variant_void);
                    auto count = base_count;
                    count += Render::GL::carthage_heavy_helmet_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count +=
                        Render::GL::carthage_shoulder_cover_fill_role_colors(
                            v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::spear_fill_role_colors(
                        v.palette, k_canonical_spear_cfg, out + count,
                        max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::armor_light_carthage_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    return count;
                  });

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/spearman";
      s.scaling = ProportionScaling{0.72F, 1.02F, 0.74F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = k_archetype;
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

    auto next_rand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0xBEEFFAU;
    bool wants_beard = style.force_beard;
    if (!wants_beard) {
      float const beard_roll = next_rand(beard_seed);
      wants_beard = (beard_roll < 0.90F);
    }

    if (wants_beard) {
      float const style_roll = next_rand(beard_seed);

      if (style_roll < 0.55F) {
        v.facial_hair.style = FacialHairStyle::FullBeard;
        v.facial_hair.length = 1.0F + next_rand(beard_seed) * 0.7F;
      } else if (style_roll < 0.80F) {
        v.facial_hair.style = FacialHairStyle::LongBeard;
        v.facial_hair.length = 1.3F + next_rand(beard_seed) * 0.9F;
      } else {
        v.facial_hair.style = FacialHairStyle::ShortBeard;
        v.facial_hair.length = 0.9F + next_rand(beard_seed) * 0.5F;
      }

      float const color_roll = next_rand(beard_seed);
      if (color_roll < 0.60F) {
        v.facial_hair.color = QVector3D(0.18F + next_rand(beard_seed) * 0.10F,
                                        0.14F + next_rand(beard_seed) * 0.08F,
                                        0.10F + next_rand(beard_seed) * 0.06F);
      } else if (color_roll < 0.85F) {
        v.facial_hair.color = QVector3D(0.30F + next_rand(beard_seed) * 0.12F,
                                        0.24F + next_rand(beard_seed) * 0.10F,
                                        0.16F + next_rand(beard_seed) * 0.08F);
      } else {
        v.facial_hair.color = QVector3D(0.35F + next_rand(beard_seed) * 0.10F,
                                        0.20F + next_rand(beard_seed) * 0.08F,
                                        0.12F + next_rand(beard_seed) * 0.06F);
      }

      v.facial_hair.thickness = 0.95F + next_rand(beard_seed) * 0.30F;
      v.facial_hair.coverage = 0.80F + next_rand(beard_seed) * 0.20F;

      if (next_rand(beard_seed) < 0.12F) {
        v.facial_hair.greyness = 0.12F + next_rand(beard_seed) * 0.30F;
      } else {
        v.facial_hair.greyness = 0.0F;
      }
    } else {
      v.facial_hair.style = FacialHairStyle::None;
    }
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const SpearmanStyleConfig & {
    ensure_spearman_styles_registered();
    auto &styles = spearman_style_registry();
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
    auto it_default = styles.find(std::string(k_spearman_default_style_key));
    if (it_default != styles.end()) {
      return it_default->second;
    }
    static const SpearmanStyleConfig k_empty{};
    return k_empty;
  }

private:
  void apply_palette_overrides(const SpearmanStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_spearman_team_mix_weight,
                                 k_spearman_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
  }
};

void register_spearman_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_spearman_styles_registered();
  static SpearmanRenderer const renderer;
  registry.register_renderer("troops/carthage/spearman",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static SpearmanRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
