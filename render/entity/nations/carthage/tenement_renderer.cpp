#include "render/entity/nations/carthage/tenement_renderer.h"

#include "render/entity/nations/carthage/tenement_authoring.h"
#include "render/entity/registry.h"
#include "render/entity/tenement_renderer_common.h"
#include "render/entity/town_palette.h"

namespace Render::GL::Carthage {
void register_tenement_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_tenement_renderer_variant(
      registry,
      TenementRendererConfig{.nation_slug = "carthage",
                             .archetype_prefix = "carthage_tenement_variant_",
                             .palette = &carthage_town_palette,
                             .vent = &tenement_vent,
                             .carthage = true});
}

} // namespace Render::GL::Carthage
