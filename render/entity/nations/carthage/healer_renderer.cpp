#include "healer_renderer.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "../equipment_loadout_catalog.h"
#include "healer_style.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_default_style_key = "default";

auto style_registry() -> std::unordered_map<std::string, HealerStyleConfig>& {
  static std::unordered_map<std::string, HealerStyleConfig> styles;
  return styles;
}

void ensure_healer_styles_registered() {
  static const bool registered = []() {
    register_carthage_healer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

} // namespace

void register_healer_style(const std::string& nation_id,
                           const HealerStyleConfig& style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class HealerRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {

    return {0.88F, 0.99F, 0.90F};
  }

  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    using namespace Render::Creature::Pipeline;

    ensure_healer_styles_registered();

    static const UnitVisualSpec spec = []() {
      const auto loadout =
          Render::GL::Nation::resolve_equipment_loadout("troops/carthage/healer");
      const std::array<EquipmentHandle, 1> handles{loadout.armor_handle};

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/healer";
      s.scaling = ProportionScaling{0.88F, 0.99F, 0.90F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = resolve_humanoid_equipment_archetype(
          "troops/carthage/healer",
          Render::Creature::ArchetypeRegistry::k_humanoid_base,
          handles);
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const& style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);

    auto next_rand = [](uint32_t& s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0x0EA101U;
    bool wants_beard = style.force_beard;
    if (!wants_beard) {
      float const beard_roll = next_rand(beard_seed);
      wants_beard = (beard_roll < 0.85F);
    }

    if (wants_beard) {
      float const style_roll = next_rand(beard_seed);

      if (style_roll < 0.45F) {
        v.facial_hair.style = FacialHairStyle::ShortBeard;
        v.facial_hair.length = 0.8F + next_rand(beard_seed) * 0.4F;
      } else if (style_roll < 0.75F) {
        v.facial_hair.style = FacialHairStyle::FullBeard;
        v.facial_hair.length = 0.9F + next_rand(beard_seed) * 0.5F;
      } else if (style_roll < 0.90F) {
        v.facial_hair.style = FacialHairStyle::Goatee;
        v.facial_hair.length = 0.7F + next_rand(beard_seed) * 0.4F;
      } else {
        v.facial_hair.style = FacialHairStyle::MustacheAndBeard;
        v.facial_hair.length = 1.0F + next_rand(beard_seed) * 0.4F;
      }

      float const color_roll = next_rand(beard_seed);
      if (color_roll < 0.55F) {

        v.facial_hair.color = QVector3D(0.12F + next_rand(beard_seed) * 0.08F,
                                        0.10F + next_rand(beard_seed) * 0.06F,
                                        0.08F + next_rand(beard_seed) * 0.05F);
      } else if (color_roll < 0.80F) {

        v.facial_hair.color = QVector3D(0.22F + next_rand(beard_seed) * 0.10F,
                                        0.17F + next_rand(beard_seed) * 0.08F,
                                        0.12F + next_rand(beard_seed) * 0.06F);
      } else {

        v.facial_hair.color = QVector3D(0.35F + next_rand(beard_seed) * 0.15F,
                                        0.32F + next_rand(beard_seed) * 0.12F,
                                        0.30F + next_rand(beard_seed) * 0.10F);
        v.facial_hair.greyness = 0.3F + next_rand(beard_seed) * 0.4F;
      }

      v.facial_hair.thickness = 0.85F + next_rand(beard_seed) * 0.25F;
      v.facial_hair.coverage = 0.80F + next_rand(beard_seed) * 0.20F;
    }
  }

private:
  auto resolve_style(const DrawContext& ctx) const -> const HealerStyleConfig& {
    ensure_healer_styles_registered();
    auto& styles = style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
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
    static const HealerStyleConfig default_style{};
    return default_style;
  }

private:
  void apply_palette_overrides(const HealerStyleConfig& style,
                               const QVector3D& team_tint,
                               HumanoidVariant& variant) const {
    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target,
                           float team_weight,
                           float style_weight) {
      target = mix_palette_color(
          target, override_color, team_tint, team_weight, style_weight);
    };

    constexpr float k_skin_team_mix_weight = 0.0F;
    constexpr float k_skin_style_mix_weight = 1.0F;

    constexpr float k_cloth_team_mix_weight = 0.0F;
    constexpr float k_cloth_style_mix_weight = 1.0F;

    apply_color(style.skin_color,
                variant.palette.skin,
                k_skin_team_mix_weight,
                k_skin_style_mix_weight);
    apply_color(style.cloth_color,
                variant.palette.cloth,
                k_cloth_team_mix_weight,
                k_cloth_style_mix_weight);
    apply_color(style.leather_color,
                variant.palette.leather,
                k_team_mix_weight,
                k_style_mix_weight);
    apply_color(style.leather_dark_color,
                variant.palette.leather_dark,
                k_team_mix_weight,
                k_style_mix_weight);
    apply_color(style.metal_color,
                variant.palette.metal,
                k_team_mix_weight,
                k_style_mix_weight);
    apply_color(
        style.wood_color, variant.palette.wood, k_team_mix_weight, k_style_mix_weight);
  }
};

void register_healer_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_healer_styles_registered();
  static HealerRenderer const renderer;
  registry.register_renderer("troops/carthage/healer",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static HealerRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
