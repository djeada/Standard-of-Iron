#pragma once

#include "i_render_pass.h"
#include <vector>

namespace Render::GL {

class Renderer;
class ResourceManager;
class GroundRenderer;
class TerrainRenderer;
class BiomeRenderer;
class RiverRenderer;
class RoadRenderer;
class RiverbankRenderer;
class BridgeRenderer;
class FogRenderer;
class StoneRenderer;
class PlantRenderer;
class PineRenderer;
class OliveRenderer;
class FireCampRenderer;
class RainRenderer;

class TerrainSceneProxy {
public:
  TerrainSceneProxy(GroundRenderer *ground, TerrainRenderer *terrain,
                    RiverRenderer *river, RoadRenderer *road,
                    RiverbankRenderer *riverbank, BridgeRenderer *bridge,
                    BiomeRenderer *biome, StoneRenderer *stone,
                    PlantRenderer *plant, PineRenderer *pine,
                    OliveRenderer *olive, FireCampRenderer *firecamp,
                    RainRenderer *rain, FogRenderer *fog)
      : m_ground(ground), m_terrain(terrain), m_river(river), m_road(road),
        m_riverbank(riverbank), m_bridge(bridge), m_biome(biome),
        m_stone(stone), m_plant(plant), m_pine(pine), m_olive(olive),
        m_firecamp(firecamp), m_rain(rain), m_fog(fog),
        m_passes{ground, terrain, river, road, riverbank, bridge, biome, stone,
                 plant, pine, olive, firecamp, rain, fog} {}

  // Submits each terrain pass through the shared terrain scene boundary.
  // Null entries are tolerated so legacy bootstrap or tests can omit passes
  // while preserving the established terrain submission order.
  void submit(Renderer &renderer, ResourceManager *resources) const {
    for (auto *pass : m_passes) {
      if (pass != nullptr) {
        pass->submit(renderer, resources);
      }
    }
  }

  [[nodiscard]] auto ground() const -> GroundRenderer * { return m_ground; }
  [[nodiscard]] auto terrain() const -> TerrainRenderer * { return m_terrain; }
  [[nodiscard]] auto river() const -> RiverRenderer * { return m_river; }
  [[nodiscard]] auto road() const -> RoadRenderer * { return m_road; }
  [[nodiscard]] auto riverbank() const -> RiverbankRenderer * {
    return m_riverbank;
  }
  [[nodiscard]] auto bridge() const -> BridgeRenderer * { return m_bridge; }
  [[nodiscard]] auto biome() const -> BiomeRenderer * { return m_biome; }
  [[nodiscard]] auto stone() const -> StoneRenderer * { return m_stone; }
  [[nodiscard]] auto plant() const -> PlantRenderer * { return m_plant; }
  [[nodiscard]] auto pine() const -> PineRenderer * { return m_pine; }
  [[nodiscard]] auto olive() const -> OliveRenderer * { return m_olive; }
  [[nodiscard]] auto firecamp() const -> FireCampRenderer * {
    return m_firecamp;
  }
  [[nodiscard]] auto rain() const -> RainRenderer * { return m_rain; }
  [[nodiscard]] auto fog() const -> FogRenderer * { return m_fog; }

  // Exposes the ordered pass list for focused tests and adapter code that
  // still needs to inspect the legacy terrain pass sequence directly.
  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass *> & {
    return m_passes;
  }

private:
  GroundRenderer *m_ground = nullptr;
  TerrainRenderer *m_terrain = nullptr;
  RiverRenderer *m_river = nullptr;
  RoadRenderer *m_road = nullptr;
  RiverbankRenderer *m_riverbank = nullptr;
  BridgeRenderer *m_bridge = nullptr;
  BiomeRenderer *m_biome = nullptr;
  StoneRenderer *m_stone = nullptr;
  PlantRenderer *m_plant = nullptr;
  PineRenderer *m_pine = nullptr;
  OliveRenderer *m_olive = nullptr;
  FireCampRenderer *m_firecamp = nullptr;
  RainRenderer *m_rain = nullptr;
  FogRenderer *m_fog = nullptr;
  std::vector<IRenderPass *> m_passes;
};

} // namespace Render::GL
