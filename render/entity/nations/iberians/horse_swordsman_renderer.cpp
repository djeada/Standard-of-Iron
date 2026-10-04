#include "horse_swordsman_renderer.h"

#include <memory>

#include "palette.h"
#include "render/entity/mounted_swordsman_renderer_base.h"
#include "render/submitter.h"

namespace Render::GL::Iberians {
namespace {

auto make_mounted_swordsman_config() -> MountedSwordsmanRendererConfig {
  auto config =
      make_mounted_swordsman_config_from_loadout("troops/iberians/horse_swordsman");
  config.rider_style = troop_style();
  config.metal_color = *config.rider_style->metal_color;
  return config;
}

} // namespace

void register_mounted_swordsman_renderer(EntityRendererRegistry& registry) {
  register_humanoid_renderer(registry,
                             "troops/iberians/horse_swordsman",
                             std::make_shared<MountedSwordsmanRendererBase const>(
                                 make_mounted_swordsman_config()));
}

} // namespace Render::GL::Iberians
