#include "render/backend/command_executor_common.h"

namespace Render::GL {

void Backend::execute_scatter_commands(const PreparedBatch& prepared,
                                       PipelineDrawContext& context) {
  const auto& cmd = context.queue.get_sorted(prepared.start);
  switch (cmd.index()) {
  case TerrainScatterCmdIndex: {
    const auto& deco_cmd_ = std::get<TerrainScatterCmdIndex>(cmd);
    switch (deco_cmd_.species) {
    case TerrainScatterCmd::Species::Grass:
      m_terrain_pipeline->draw_grass(deco_cmd_, context);
      break;
    case TerrainScatterCmd::Species::Stone:
      if (m_vegetation_pipeline) {
        m_vegetation_pipeline->draw_stone(deco_cmd_, context);
      }
      break;
    case TerrainScatterCmd::Species::Plant:
    case TerrainScatterCmd::Species::Pine:
    case TerrainScatterCmd::Species::Olive:
    case TerrainScatterCmd::Species::Cypress:
    case TerrainScatterCmd::Species::Palm:
      if (m_vegetation_pipeline) {
        m_vegetation_pipeline->draw_foliage(deco_cmd_, context);
      }
      break;
    case TerrainScatterCmd::Species::FireCamp:
      if (m_vegetation_pipeline) {
        m_vegetation_pipeline->draw_firecamp(deco_cmd_, context);
      }
      break;
    case TerrainScatterCmd::Species::Tent:
    case TerrainScatterCmd::Species::SupplyCart:
    case TerrainScatterCmd::Species::WeaponRack:
    case TerrainScatterCmd::Species::Ruins:
    case TerrainScatterCmd::Species::DeadTree:
    case TerrainScatterCmd::Species::IronOre:
    case TerrainScatterCmd::Species::MagicShrine:
    case TerrainScatterCmd::Species::CursedGoldVein:
    case TerrainScatterCmd::Species::AbandonedHome:
    case TerrainScatterCmd::Species::Statue:
      if (m_vegetation_pipeline) {
        m_vegetation_pipeline->draw_prop(deco_cmd_, context);
      }
      break;
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
