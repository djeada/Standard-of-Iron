// Helmets and cloaks of the historical cameo commanders (issue #1522). Each
// cameo reuses the commander helmet shells, the shared cloak mesh and the
// playable commanders' armour, weapons and regalia; only the crest, plume or
// diadem and the cloak colours are new.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "armor/cloak_renderer.h"
#include "helmets/commander_helmets.h"
#include "register_equipment_internal.h"
#include "render_archetype_registry.h"

namespace Render::GL::EquipmentRegistration {
namespace {

template <CommanderHelmetStyle Style>
auto build_cameo_helmet(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::commander_helmet_make_static_attachment(
      Style, humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix())};
}

template <CommanderHelmetStyle Style>
auto cameo_helmet_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::commander_helmet_fill_role_colors(
                           Style, variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

enum class CameoCloak : std::uint8_t {
  Sempronius,
  Flaminius,
  Varro,
  Paullus,
  ScipioElder,
  Mago,
  Maharbal,
  HannoBomilcar,
  HasdrubalCavalry,
  Masinissa,
};

auto make_cloak(QVector3D primary,
                QVector3D trim,
                float length_scale,
                float width_scale,
                float shoulder_anchor_up,
                bool show_clasp) -> CloakConfig {
  CloakConfig cfg;
  cfg.primary_color = primary;
  cfg.trim_color = trim;
  cfg.length_scale = length_scale;
  cfg.width_scale = width_scale;
  cfg.shoulder_anchor_up = shoulder_anchor_up;
  cfg.show_clasp = show_clasp;
  cfg.team_blend = 0.12F;
  cfg.team_shade = 0.96F;
  cfg.back_material_id = 12;
  cfg.shoulder_material_id = 13;
  return cfg;
}

auto cameo_cloak_config(CameoCloak cloak) -> const CloakConfig& {
  static const std::array<CloakConfig, 10> configs{{
      make_cloak(
          {0.64F, 0.13F, 0.045F}, {0.92F, 0.70F, 0.26F}, 1.08F, 0.94F, 0.07F, true),
      make_cloak(
          {0.74F, 0.53F, 0.12F}, {0.30F, 0.20F, 0.10F}, 0.92F, 0.88F, 0.10F, true),
      make_cloak(
          {0.34F, 0.06F, 0.38F}, {0.94F, 0.74F, 0.28F}, 1.16F, 1.04F, 0.07F, true),
      make_cloak(
          {0.44F, 0.20F, 0.10F}, {0.80F, 0.80F, 0.82F}, 1.06F, 0.92F, 0.07F, true),
      make_cloak(
          {0.20F, 0.32F, 0.50F}, {0.90F, 0.88F, 0.82F}, 1.02F, 0.92F, 0.08F, true),
      make_cloak(
          {0.07F, 0.30F, 0.14F}, {0.90F, 0.68F, 0.24F}, 1.06F, 0.92F, 0.08F, true),
      make_cloak(
          {0.04F, 0.36F, 0.38F}, {0.88F, 0.84F, 0.70F}, 0.84F, 0.86F, 0.12F, false),
      make_cloak(
          {0.12F, 0.12F, 0.42F}, {0.80F, 0.80F, 0.84F}, 0.80F, 0.86F, 0.12F, false),
      make_cloak(
          {0.05F, 0.05F, 0.06F}, {0.70F, 0.46F, 0.18F}, 1.04F, 0.96F, 0.08F, true),
      make_cloak(
          {0.88F, 0.85F, 0.76F}, {0.58F, 0.06F, 0.06F}, 0.62F, 0.80F, 0.14F, false),
  }};
  return configs[static_cast<std::size_t>(cloak)];
}

template <CameoCloak Cloak>
auto build_cameo_cloak(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  auto const& config = cameo_cloak_config(Cloak);
  return {
      Render::GL::cloak_make_static_attachment(config,
                                               Render::GL::shared_cloak_meshes(config),
                                               humanoid_chest_bone(),
                                               base_role_byte)};
}

template <CameoCloak Cloak>
auto cameo_cloak_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_cloak_role_count) {
          return count;
        }
        auto const& config = cameo_cloak_config(Cloak);
        colors[count] = Render::GL::cloak_mantle_color(config, variant.palette);
        colors[count + 1U] = config.trim_color;
        return count + Render::GL::k_cloak_role_count;
      },
      out,
      base_count,
      max_count);
}

template <CommanderHelmetStyle Style, CameoCloak Cloak>
void register_cameo_equipment(const char* name) {
  auto& registry = EquipmentRegistry::instance();
  std::string const helmet_id = std::string("commander_") + name;
  std::string const cloak_id = std::string("cloak_") + name;
  registry.register_equipment_id(EquipmentCategory::Helmet, helmet_id);
  registry.register_equipment_id(EquipmentCategory::Armor, cloak_id);
  register_humanoid_equipment_contribution(
      registry.resolve_handle(EquipmentCategory::Helmet, helmet_id),
      {.build_attachments = &build_cameo_helmet<Style>,
       .append_role_colors = &cameo_helmet_role_colors<Style>,
       .role_count = static_cast<std::uint8_t>(k_commander_helmet_role_count)});
  register_humanoid_equipment_contribution(
      registry.resolve_handle(EquipmentCategory::Armor, cloak_id),
      {.build_attachments = &build_cameo_cloak<Cloak>,
       .append_role_colors = &cameo_cloak_role_colors<Cloak>,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  RenderArchetypeRegistry::instance().register_archetype(
      std::string("commander_helmet_") + name,
      [] { (void)commander_helmet_archetype(Style); });
}

} // namespace

void register_historical_commander_equipment() {
  using H = CommanderHelmetStyle;
  using C = CameoCloak;
  register_cameo_equipment<H::Sempronius, C::Sempronius>("sempronius");
  register_cameo_equipment<H::Flaminius, C::Flaminius>("flaminius");
  register_cameo_equipment<H::Varro, C::Varro>("varro");
  register_cameo_equipment<H::Paullus, C::Paullus>("paullus");
  register_cameo_equipment<H::ScipioElder, C::ScipioElder>("scipio_elder");
  register_cameo_equipment<H::Mago, C::Mago>("mago");
  register_cameo_equipment<H::Maharbal, C::Maharbal>("maharbal");
  register_cameo_equipment<H::HannoBomilcar, C::HannoBomilcar>("hanno_bomilcar");
  register_cameo_equipment<H::HasdrubalCavalry, C::HasdrubalCavalry>(
      "hasdrubal_cavalry");
  register_cameo_equipment<H::Masinissa, C::Masinissa>("masinissa");
}

} // namespace Render::GL::EquipmentRegistration
