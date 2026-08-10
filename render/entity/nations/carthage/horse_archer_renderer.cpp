#include "horse_archer_renderer.h"

#include "render/entity/horse_archer_renderer_base.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/nations/mounted_loadout.h"
#include "render/submitter.h"

namespace Render::GL::Carthage {
namespace {

auto make_horse_archer_config() -> HorseArcherRendererConfig {
  HorseArcherRendererConfig config;
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_archer");
  Render::GL::Nation::apply_mount_loadout(
      config, loadout, "troops/carthage/horse_archer");
  config.bow_equipment_id = loadout.ids.bow;
  config.quiver_equipment_id = loadout.ids.quiver;
  config.helmet_equipment_id = loadout.ids.helmet;
  config.armor_equipment_id = loadout.ids.armor;
  config.cloak_equipment_id = "cloak_carthage_mounted";
  config.bow_handle = loadout.bow_handle;
  config.quiver_handle = loadout.quiver_handle;
  config.helmet_handle = loadout.helmet_handle;
  config.armor_handle = loadout.armor_handle;
  config.has_cloak = !loadout.ids.cloak.empty();
  return config;
}

} // namespace

void register_horse_archer_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer("troops/carthage/horse_archer",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static HorseArcherRendererBase const static_renderer(
                                   make_horse_archer_config());
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
