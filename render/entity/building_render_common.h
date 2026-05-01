#pragma once

#include "../../game/systems/nation_id.h"
#include "../render_archetype.h"
#include "building_state.h"
#include "registry.h"
#include <span>
#include <string>
#include <string_view>

namespace Render::GL {

struct BuildingHealthBarStyle {
  float width{1.0F};
  float height{0.08F};
  float y{1.5F};
  bool draw_segment_highlights{false};
};

struct BuildingSelectionStyle {
  float scale_x{1.5F};
  float scale_z{1.5F};
};

auto resolve_building_health_ratio(const DrawContext &ctx) -> float;
auto resolve_building_state(const DrawContext &ctx) -> BuildingState;
auto building_renderer_key(std::string_view nation_slug,
                           std::string_view building_type) -> std::string;
auto building_renderer_key(Game::Systems::NationID nation_id,
                           std::string_view building_type) -> std::string;
auto canonicalize_building_renderer_key(std::string_view renderer_key)
    -> std::string_view;
auto resolve_building_renderer_key(
    std::string_view renderer_key, std::string_view building_type,
    Game::Systems::NationID nation_id) -> std::string;

void submit_building_instance(ISubmitter &out, const DrawContext &ctx,
                              const RenderArchetype &archetype,
                              std::span<const QVector3D> palette = {});
void submit_building_box(ISubmitter &out, Mesh *mesh, Texture *texture,
                         const QMatrix4x4 &model, const QVector3D &pos,
                         const QVector3D &size, const QVector3D &color,
                         float alpha = 1.0F);
void submit_building_cylinder(ISubmitter &out, const QMatrix4x4 &model,
                              const QVector3D &start, const QVector3D &end,
                              float radius, const QVector3D &color,
                              Texture *texture, float alpha = 1.0F);

void draw_building_health_bar(ISubmitter &out, const DrawContext &ctx,
                              const BuildingHealthBarStyle &style);
void draw_building_compact_health_bar(ISubmitter &out, const DrawContext &ctx,
                                      float y);
void draw_building_selection_overlay(ISubmitter &out, const DrawContext &ctx,
                                     const BuildingSelectionStyle &style);

auto select_nation_variant_renderer_key(
    std::string_view roman_key, std::string_view carthage_key,
    Game::Systems::NationID nation_id) -> std::string_view;
void register_nation_variant_renderer(EntityRendererRegistry &registry,
                                      const std::string &public_key,
                                      std::string roman_key,
                                      std::string carthage_key);
void register_building_renderer(EntityRendererRegistry &registry,
                                std::string_view nation_slug,
                                std::string_view building_type,
                                RenderFunc func);

} // namespace Render::GL
