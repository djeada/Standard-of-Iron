#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
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

  enum class DecorKind : std::uint8_t {
    Log,
    Firewood
  };

  struct DecorCylinder {
    QVector3D start;
    QVector3D end;
    QVector3D base_color;
    float radius = 0.0F;
    float ember_weight = 0.0F;
    DecorKind kind = DecorKind::Log;
  };

  struct DecorStone {
    QMatrix4x4 model;
    QVector3D centre;
    float half_height = 0.0F;
    QVector3D color;
  };

  struct CampDecor {
    float phase = 0.0F;
    std::vector<DecorCylinder> cylinders;
    std::vector<DecorStone> stones;
  };

  [[nodiscard]] static auto hearth_radius(float authored_scale) -> float;

  [[nodiscard]] static auto build_camp_decor(const Game::Map::TerrainService& terrain,
                                             float world_x,
                                             float world_z,
                                             float hearth_radius,
                                             float phase) -> CampDecor;

  [[nodiscard]] auto camp_decor() const -> const std::vector<CampDecor>& {
    return m_camp_decor;
  }

  [[nodiscard]] auto camps() const -> const std::vector<FireCampInstanceGpu>& {
    return m_state.instances;
  }

private:
  void generate_firecamp_instances();

  std::vector<CampDecor> m_camp_decor;
};

} // namespace Render::GL
