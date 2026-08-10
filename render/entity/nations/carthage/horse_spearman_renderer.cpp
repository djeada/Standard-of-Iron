#include "horse_spearman_renderer.h"

#include "render/entity/horse_spearman_renderer_base.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/nations/mounted_loadout.h"
#include "render/submitter.h"

namespace Render::GL::Carthage {
namespace {

auto make_horse_spearman_config() -> HorseSpearmanRendererConfig {
  HorseSpearmanRendererConfig config;
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_spearman");
  Render::GL::Nation::apply_mount_loadout(
      config, loadout, "troops/carthage/horse_spearman");
  config.spear_equipment_id = loadout.ids.spear;
  config.helmet_equipment_id = loadout.ids.helmet;
  config.armor_equipment_id = loadout.ids.armor;
  config.shoulder_equipment_id = loadout.ids.shoulder;
  config.spear_handle = loadout.spear_handle;
  config.helmet_handle = loadout.helmet_handle;
  config.armor_handle = loadout.armor_handle;
  config.shoulder_handle = loadout.shoulder_handle;
  config.has_shoulder = loadout.shoulder_handle != k_invalid_equipment_handle;
  return config;
}

} // namespace

void register_horse_spearman_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer("troops/carthage/horse_spearman",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static HorseSpearmanRendererBase const static_renderer(
                                   make_horse_spearman_config());
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
