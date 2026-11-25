#pragma once

namespace Render::GL {

class EntityRendererRegistry;

// The buildings only one nation raises (NATION_ASYMMETRY.md): Carthage's
// Mercenary Camp and Elephant Stables, Rome's Fabrica and Castra. Until they
// get their own authoring they are drawn from existing archetypes in the
// nation's palette, scaled so they read apart from a barracks or a market.
void register_nation_building_renderers(EntityRendererRegistry& registry);

} // namespace Render::GL
