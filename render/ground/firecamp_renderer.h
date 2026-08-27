#pragma once

#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "render/decoration_gpu.h"
#include "scatter_renderer_base.h"

namespace Render::GL {
class Buffer;
class Renderer;

class FireCampRenderer
    : public ScatterRendererBase<FireCampInstanceGpu, FireCampBatchParams> {
public:
  FireCampRenderer();
  ~FireCampRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {});

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void clear() override;

  struct FireLightShape {
    float height_above_ground = 0.0F;
    float reach = 0.0F;
  };
  [[nodiscard]] static auto fire_light_shape(float camp_radius) -> FireLightShape;

private:
  struct DecorCylinder {
    QVector3D start;
    QVector3D end;
    QVector3D base_color;
    float radius = 0.0F;
    float ember_weight = 0.0F;
  };

  struct CampDecor {
    float phase = 0.0F;
    std::vector<DecorCylinder> cylinders;
  };

  void generate_firecamp_instances();
  void build_camp_decor(const QVector3D& camp_pos,
                        float base_radius,
                        float phase,
                        CampDecor& decor) const;

  std::vector<CampDecor> m_camp_decor;
};

} // namespace Render::GL
