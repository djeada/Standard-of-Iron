#include "render/entity/nations/roman/horse_swordsman_renderer.h"

#include <memory>

#include "render/entity/mounted_swordsman_renderer_base.h"
#include "render/entity/nations/material_palette.h"
#include "render/submitter.h"

namespace Render::GL::Roman {
namespace {

auto make_mounted_swordsman_config() -> MountedSwordsmanRendererConfig {
  auto config =
      make_mounted_swordsman_config_from_loadout("troops/roman/horse_swordsman");
  const auto& materials = Nation::k_roman_materials;
  Nation::apply_materials(config.rider_style.emplace(), materials);
  config.metal_color = materials.metal;
  return config;
}

} // namespace

void register_mounted_swordsman_renderer(EntityRendererRegistry& registry) {
  register_humanoid_renderer(registry,
                             "troops/roman/horse_swordsman",
                             std::make_shared<MountedSwordsmanRendererBase const>(
                                 make_mounted_swordsman_config()));
}

} // namespace Render::GL::Roman
