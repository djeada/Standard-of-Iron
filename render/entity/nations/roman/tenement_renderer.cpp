#include "render/entity/nations/roman/tenement_renderer.h"

#include "render/entity/nations/roman/tenement_authoring.h"
#include "render/entity/registry.h"
#include "render/entity/tenement_renderer_common.h"
#include "render/entity/town_palette.h"

namespace Render::GL::Roman {
void register_tenement_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_tenement_renderer_variant(
      registry,
      TenementRendererConfig{.nation_slug = "roman",
                             .archetype_prefix = "roman_tenement_variant_",
                             .palette = &roman_town_palette,
                             .vent = &tenement_vent,
                             .carthage = false});
}

} // namespace Render::GL::Roman
