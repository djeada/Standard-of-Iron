#pragma once

#include <vector>

#include "game/map/terrain.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/scatter/scatter_request.h"
#include "render/ground/tree_renderer.h"

namespace Render::Ground::Scatter {

struct GrassScatter {
  std::vector<Render::GL::GrassInstanceGpu> instances;

  float generated_density = -1.0F;

  float typical_blade_height = -1.0F;
};

struct FireCampScatter {
  std::vector<Render::GL::FireCampInstanceGpu> instances;

  std::vector<Render::GL::FireCampRenderer::CampDecor> decor;
};

auto generate_grass(const ScatterRequest& request,
                    float requested_density) -> GrassScatter;
auto generate_stones(const ScatterRequest& request)
    -> std::vector<Render::GL::StoneInstanceGpu>;
auto generate_plants(const ScatterRequest& request)
    -> std::vector<Render::GL::PlantInstanceGpu>;
auto generate_trees(const ScatterRequest& request,
                    Game::Map::TreeSpecies species,
                    const std::vector<Render::GL::AnchoredTree>& anchored)
    -> std::vector<Render::GL::TreeInstanceGpu>;
auto generate_firecamps(const ScatterRequest& request) -> FireCampScatter;
auto generate_boulders(const ScatterRequest& request)
    -> std::vector<Render::GL::StoneInstanceGpu>;

auto generate_tents(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_supply_carts(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_weapon_racks(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_ruins(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_dead_trees(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_iron_ore(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_magic_shrines(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_abandoned_homes(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_statues(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;
auto generate_cursed_gold_veins(const ScatterRequest& request)
    -> std::vector<Render::GL::PropInstanceGpu>;

} // namespace Render::Ground::Scatter
