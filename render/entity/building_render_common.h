#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "building_state.h"
#include "game/systems/nation_id.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

struct BuildingSelectionStyle {
  float scale_x{1.5F};
  float scale_z{1.5F};
};

auto resolve_building_health_ratio(const DrawContext& ctx) -> float;

auto resolve_building_state(const DrawContext& ctx) -> BuildingState;

// Seconds since the structure last switched damage state on screen, measured
// on the clock of the DrawContexts that resolved it; negative when it has not
// switched since it was first drawn.
auto building_state_transition_age(std::uint32_t entity_id, float now) -> float;
auto building_renderer_key(std::string_view nation_slug,
                           std::string_view building_type) -> std::string;
auto building_renderer_key(Game::Systems::NationID nation_id,
                           std::string_view building_type) -> std::string;
auto canonicalize_building_renderer_key(std::string_view renderer_key)
    -> std::string_view;
auto resolve_building_renderer_key(std::string_view renderer_key,
                                   std::string_view building_type,
                                   Game::Systems::NationID nation_id) -> std::string;

void submit_building_instance(ISubmitter& out,
                              const DrawContext& ctx,
                              const RenderArchetype& archetype,
                              std::span<const QVector3D> palette = {});
void submit_building_box(ISubmitter& out,
                         Mesh* mesh,
                         Texture* texture,
                         const QMatrix4x4& model,
                         const QVector3D& pos,
                         const QVector3D& size,
                         const QVector3D& color,
                         float alpha = 1.0F);
void submit_building_cylinder(ISubmitter& out,
                              const QMatrix4x4& model,
                              const QVector3D& start,
                              const QVector3D& end,
                              float radius,
                              const QVector3D& color,
                              Texture* texture,
                              float alpha = 1.0F);

void draw_building_selection_overlay(ISubmitter& out,
                                     const DrawContext& ctx,
                                     const BuildingSelectionStyle& style);

auto select_nation_variant_renderer_key(std::string_view roman_key,
                                        std::string_view carthage_key,
                                        Game::Systems::NationID nation_id,
                                        std::string_view sepulcher_key = {})
    -> std::string_view;
void register_nation_variant_renderer(EntityRendererRegistry& registry,
                                      const std::string& public_key,
                                      std::string roman_key,
                                      std::string carthage_key,
                                      std::string sepulcher_key = {});
void register_building_renderer(EntityRendererRegistry& registry,
                                std::string_view nation_slug,
                                std::string_view building_type,
                                RenderFunc func);

} // namespace Render::GL
